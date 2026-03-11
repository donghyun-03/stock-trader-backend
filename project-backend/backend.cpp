#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric> 
#include <iomanip>
#include "backend.h"

//MarketData
void MarketData::LoadData(const std::string& file_path) {
    std::ifstream file(file_path);

    // 파일 열기 실패 검사
    if (!file.is_open()) {
        std::cerr << "Error: Could not open data file: " << file_path << std::endl;
        return;
    }
    historical_data_.clear();

    std::string line;
    // 헤더 행 건너뛰기 (가정: 첫 행은 "Date,Open,High,Low,Close,Volume" 같은 헤더)
    if (std::getline(file, line)) {
        std::cout << "Skipping header: " << line << std::endl;
    }

    // 데이터 행 읽기 및 파싱
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string segment;
        BarData bar;
        int i = 0;

        try {
            // CSV 파싱 (각 항목은 쉼표로 구분된다고 가정)
            while (std::getline(ss, segment, ',')) {
                switch (i) {
                case 0: bar.date = segment; break;
                case 1: bar.open = std::stod(segment); break;
                case 2: bar.high = std::stod(segment); break;
                case 3: bar.low = std::stod(segment); break;
                case 4: bar.close = std::stod(segment); break;
                case 5: bar.volume = static_cast<int>(std::stod(segment)); break;
                default: break;
                }
                i++;
            }

            // 데이터 벡터에 추가
            if (i == 6) { // 6개의 필드가 모두 정상적으로 읽혔는지 확인
                historical_data_.push_back(bar);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " (" << e.what() << ")" << std::endl;
        }
    }

    std::cout << "Data loaded successfully. Total bars: " << historical_data_.size() << std::endl;
    file.close();
}
const BarData* MarketData::GetBar(int index) const {
    // 인덱스가 유효 범위 내에 있는지 검사
    if (index >= 0 && index < historical_data_.size()) {
        // 유효한 인덱스일 경우 해당 데이터의 주소를 반환
        return &historical_data_[index];
    }
    else {
        // 인덱스가 범위를 벗어날 경우 nullptr 반환
        return nullptr;
    }
}

//IndicatorEngine
/**
 * @brief 전체 MarketData에 대해 지정된 기간(period)의 단순 이동평균(MA) 시계열을 계산합니다.
 * * @param data 전체 과거 시장 데이터를 담고 있는 MarketData 객체.
 * @param period 이동평균을 계산할 기간 (예: 5일, 20일).
 * @return std::vector<double> 각 날짜에 해당하는 MA 값들의 시계열 벡터.
 */
std::vector<double> IndicatorEngine::CalculateMA(const MarketData& data, int period) {

    int data_size = data.GetDataSize();
    std::vector<double> ma_values;

    // 유효성 검사: 기간이 데이터 크기보다 크거나, 0 이하인 경우
    if (period <= 0 || period > data_size) {
        std::cerr << "Error: Invalid period (" << period << ") for MA calculation." << std::endl;
        // 데이터 크기만큼 0으로 채워서 반환하거나 예외를 던질 수 있습니다.
        return std::vector<double>(data_size, 0.0);
    }

    // 1. 필요한 종가(Close Price) 데이터 추출
    std::vector<double> close_prices;
    for (int i = 0; i < data_size; ++i) {
        const BarData* bar = data.GetBar(i);
        if (bar) {
            close_prices.push_back(bar->close);
        }
        else {
            // 데이터 접근 오류 처리
            std::cerr << "Error: BarData access failed at index " << i << std::endl;
            return std::vector<double>(data_size, 0.0);
        }
    }

    // 2. 이동평균 계산
    for (int i = 0; i < data_size; ++i) {
        if (i < period - 1) {
            // 데이터가 충분하지 않은 초기 기간 (예: 5일 MA 계산인데 4일차)
            ma_values.push_back(0.0); // 초기 MA 값은 0 또는 NaN으로 처리할 수 있습니다.
        }
        else {
            // 현재 인덱스(i)를 기준으로, period 길이만큼 이전 데이터까지의 합을 구합니다.

            double sum = 0.0;

            // i - (period - 1) 부터 i 까지의 합산
            for (int j = 0; j < period; ++j) {
                sum += close_prices[i - j];
            }

            // 평균 계산 후 결과 벡터에 추가
            ma_values.push_back(sum / period);
        }
    }

    std::cout << "Successfully calculated MA series for period: " << period << std::endl;
    return ma_values;
}

//MAStrategy
/**
 * @brief 이동평균선(MA) 크로스오버 전략에 따라 매매 시그널을 생성합니다.
 * * @param data (사용되지 않음 - 데이터 접근은 indicators를 통해)
 * @param indicators IndicatorEngine이 계산한 MA 시계열을 포함하는 맵.
 * @return std::vector<int> 각 날짜에 해당하는 시그널 (-1: 매도, 0: 관망/유지, 1: 매수).
 */
std::vector<int> MAStrategy::GenerateSignal(const MarketData& data,
    const std::map<std::string, std::vector<double>>& indicators) {

    // 1. 필요한 MA 시계열 데이터 확보
    // 키 이름은 IndicatorEngine에서 정의한 방식과 일치해야 합니다. (예: "MA_5", "MA_20")
    std::string short_key = "MA_" + std::to_string(short_period_);
    std::string long_key = "MA_" + std::to_string(long_period_);

    if (indicators.find(short_key) == indicators.end() || indicators.find(long_key) == indicators.end()) {
        std::cerr << "Error: Required MA indicators not found in map." << std::endl;
        return std::vector<int>(data.GetDataSize(), 0); // 실패 시 모두 관망(0) 반환
    }

    const std::vector<double>& short_ma = indicators.at(short_key);
    const std::vector<double>& long_ma = indicators.at(long_key);

    int data_size = short_ma.size();
    std::vector<int> signals(data_size, 0); // 초기 시그널은 모두 0 (관망)

    // 2. 시그널 생성 로직 실행
    // 데이터가 충분하지 않은 초기 기간은 건너뜁니다 (MA 값이 0인 구간)
    // 두 MA 중 더 긴 기간이 유효한 시점부터 비교를 시작합니다.
    int start_index = std::max(short_period_, long_period_) - 1;

    for (int i = start_index; i < data_size; ++i) {

        // 현재 시점의 단기 MA와 장기 MA 값
        double current_short_ma = short_ma[i];
        double current_long_ma = long_ma[i];

        // 이전 시점의 단기 MA와 장기 MA 값 (크로스오버 감지를 위함)
        double prev_short_ma = short_ma[i - 1];
        double prev_long_ma = long_ma[i - 1];

        // === [매수 시그널] ===
        // 단기 MA가 장기 MA를 아래에서 위로 돌파했을 때 (골든 크로스)
        if (prev_short_ma <= prev_long_ma && current_short_ma > current_long_ma) {
            signals[i] = 1; // 매수 (Buy)
        }
        // === [매도 시그널] ===
        // 단기 MA가 장기 MA를 위에서 아래로 하향 돌파했을 때 (데드 크로스)
        else if (prev_short_ma >= prev_long_ma && current_short_ma < current_long_ma) {
            signals[i] = -1; // 매도 (Sell)
        }
        // 그 외에는 0 (관망 또는 포지션 유지)
    }

    std::cout << "MA Strategy signals generated successfully." << std::endl;
    return signals;
}

//SignalGenerator
std::vector<int> SignalGenerator::GetSignals(const MarketData& data,
    const std::map<std::string, std::vector<double>>& indicators) {
    if (current_strategy_ == nullptr) {
        std::cerr << "Error: Strategy is not defined." << std::endl;
        return std::vector<int>(data.GetDataSize(), 0);
    }

    // 1. 전략으로부터 순수한 시그널 벡터를 얻음
    std::vector<int> signals = current_strategy_->GenerateSignal(data, indicators);

    // 2. SignalData 구조체에 날짜와 시그널을 담아 저장
    signal_history_.clear();
    for (size_t i = 0; i < signals.size(); ++i) {
        const BarData* bar = data.GetBar(static_cast<int>(i));
        if (bar) {
            // SignalData 리스트에 모든 날짜의 신호를 저장 (신호 0인 날 포함)
            signal_history_.push_back({ bar->date, signals[i] });
        }
    }
    return signals;
}

//ExecutionModel
/**
 * @brief 시그널을 바탕으로 실제 매매 주문을 처리하고 TradeRecord를 생성합니다.
 *
 * @param signal SignalGenerator로부터 받은 시그널 (1: 매수, -1: 매도, 0: 관망).
 * @param current_bar 현재 날짜의 가격 데이터 (종가, 날짜 정보를 사용).
 * @param current_holding_price 현재 보유 포지션의 평단가. (매수 수량 결정 시 사용)
 * @param current_shares_held 현재 보유 중인 주식 수량.
 * @return TradeRecord 생성된 매매 기록. 매매가 없을 경우 quantity=0인 기록 반환.
 */
TradeRecord ExecutionModel::ExecuteOrder(int signal, const BarData& current_bar,
    double current_holding_price, int current_shares_held) {

    // 기본적으로 거래가 없는 TradeRecord를 생성
    TradeRecord record = {
        current_bar.date,
        "HOLD",
        current_bar.close,
        0,
        0.0
    };

    // 현재 종가에 슬리피지를 적용하여 예상 체결 가격을 계산합니다.
    double base_price = current_bar.close;
    double execution_price = base_price;
    double trade_amount = 0.0; // 거래 총액 (수수료 계산용)
    int trade_quantity = 0;   // 실제 거래 수량

    // ===============================================
    // 1. 매수 시그널 처리 (signal == 1)
    // ===============================================
    if (signal == 1) {
        // 이미 주식을 보유(롱포지션) 중이라면, 추가 매매는 하지 않고 관망(HOLD)합니다.
        // (단순 MA 크로스오버 전략에서는 포지션을 하나만 유지하는 것이 일반적입니다.)
        if (current_shares_held > 0) {
            return record; // 거래 없음
        }

        // [체결 가격 계산] 매수 시 슬리피지(비용)를 반영하여 더 높은 가격으로 체결 가정
        execution_price = base_price * (1.0 + slippage_);

        // [거래 수량 결정] 임의로 초기 자본의 95% 구매
        trade_quantity = (int)(cash_ * 0.95) / execution_price;

        // [거래 총액]
        trade_amount = execution_price * trade_quantity;

        // [수수료 계산]
        double fee = trade_amount * fee_rate_;

        // TradeRecord 업데이트
        record.type = "BUY";
        record.price = execution_price;
        record.quantity = trade_quantity;
        record.fee = fee;
    }
    // ===============================================
    // 2. 매도 시그널 처리 (signal == -1)
    // ===============================================
    else if (signal == -1) {
        // 주식을 보유하고 있지 않다면 매도는 불가능합니다. (노포지션)
        if (current_shares_held == 0) {
            return record; // 거래 없음
        }

        // [체결 가격 계산] 매도 시 슬리피지(손해)를 반영하여 더 낮은 가격으로 체결 가정
        execution_price = base_price * (1.0 - slippage_);

        // [거래 수량 결정] 현재 보유 중인 모든 주식을 매도합니다. (청산)
        trade_quantity = current_shares_held;

        // [거래 총액]
        trade_amount = execution_price * trade_quantity;

        // [수수료 계산]
        double fee = trade_amount * fee_rate_;

        // TradeRecord 업데이트
        record.type = "SELL";
        record.price = execution_price;
        record.quantity = trade_quantity;
        record.fee = fee;
    }

    return record;
}

//PositionManager
/**
 * @brief 거래 기록에 따라 롱/숏 포지션 상태 및 자금 잔고를 갱신합니다.
 * @param record ExecutionModel에서 넘어온 실제 체결된 거래 기록.
 */
void PositionManager::UpdatePosition(const TradeRecord& record) {

    // 1. 거래 수량 0 체크 (거래가 없으면 종료)
    if (record.quantity <= 0) {
        return;
    }

    // 2. 공통 처리: 거래 기록 저장 및 수수료 차감
    trade_history_.push_back(record);
    cash_balance_ -= record.fee;

    // 현재 포지션 상태 스냅샷
    int old_shares = shares_held_;
    double old_total_value = average_price_ * std::abs(old_shares); // 이전 포지션의 총 가치/부채 금액
    double transaction_amount = record.price * record.quantity;

    // =======================================================
    // BUY (매수) 거래 처리: 주식 수량 증가, 현금 감소
    // =======================================================
    if (record.type == "BUY") {

        cash_balance_ -= transaction_amount; // 현금 감소 (매수 비용)

        if (old_shares <= 0) {
            // Case 1: Short Cover (숏 포지션 청산)

            // 주식 수량 갱신 (음수였던 shares_held가 0에 가까워지거나 양수로 넘어감)
            shares_held_ += record.quantity;

            if (shares_held_ > 0) {
                // [숏 -> 롱 전환]: 숏 포지션 전량 청산 후, 초과된 수량만큼 롱 진입
                // 숏 포지션의 P&L은 cash_balance에 반영됨.

                // 초과된 롱 수량에 대한 평균 매입가 설정
                average_price_ = record.price;
                position_status_ = "LONG";

            }
            else if (shares_held_ == 0) {
                // 숏 포지션 완전 청산
                position_status_ = "NONE";
                average_price_ = 0.0;
            }
            else { // shares_held_ < 0 (부분 숏 커버)
                // 숏 포지션 유지. 평균 매도가(average_price_)는 바뀌지 않음.
                position_status_ = "SHORT";
            }
        }

        else { // old_shares > 0
            // Case 2: Long Add (롱 포지션 추가 매수)

            int new_total_shares = old_shares + record.quantity;
            double new_total_value = old_total_value + transaction_amount;

            shares_held_ = new_total_shares;
            average_price_ = new_total_value / shares_held_; // 가중 평균 단가 갱신
            position_status_ = "LONG";
        }
    }

    // =======================================================
    // SELL (매도) 거래 처리: 주식 수량 감소, 현금 증가
    // =======================================================
    else if (record.type == "SELL") {

        cash_balance_ += transaction_amount; // 현금 증가 (판매 대금)

        if (old_shares >= 0) {
            // Case 3: Long Exit (롱 포지션 청산) 또는 Short Entry (숏 포지션 진입)

            // 주식 수량 갱신
            shares_held_ -= record.quantity;

            if (shares_held_ > 0) {
                // 부분 매도: 롱 포지션 유지. (평균 매입가는 그대로)
                position_status_ = "LONG";
            }
            else if (shares_held_ == 0) {
                // 롱 포지션 완전 청산
                position_status_ = "NONE";
                average_price_ = 0.0;
            }
            else { // shares_held_ < 0
                // [롱 -> 숏 전환]: 롱 포지션 전량 청산 후, 초과된 수량만큼 숏 진입

                // 숏 포지션의 평균 매도가 설정
                average_price_ = record.price;
                position_status_ = "SHORT";
            }
        }

        else { // old_shares < 0
            // Case 4: Short Add (숏 포지션 추가 매도)

            // 수량 갱신 (더 큰 음수로)
            int new_total_shares_abs = std::abs(old_shares) + record.quantity;

            // 숏 포지션의 총 매도 금액 증가
            double new_total_revenue = old_total_value + transaction_amount;

            shares_held_ = -new_total_shares_abs; // 음수로 설정
            average_price_ = new_total_revenue / new_total_shares_abs; // 가중 평균 매도가 갱신
            position_status_ = "SHORT";
        }
    }

    std::cout << std::fixed << std::setprecision(0) // 소수점 0자리 (정수만)
        << "Position updated: Shares=" << shares_held_
        << ", Cash=" << cash_balance_
        << ", AvgPrice=" << std::setprecision(2) << average_price_ // 평단가는 소수점 2자리
        << ", Status=" << position_status_ << std::endl;

    // 원래 설정으로 되돌리기 (선택사항)
    std::cout.unsetf(std::ios::fixed);
}

//RiskEngine
/**
 * @brief 일별 자산 가치 곡선(Equity Curve)을 기반으로 최대 손실폭(MDD)을 계산합니다.
 * * @param equity_curve 일별 포트폴리오의 총 가치 벡터.
 * @return double 최대 손실폭 (0과 1 사이의 값, 예: 0.25는 25% 하락).
 */
double RiskEngine::CalculateMDD(const std::vector<double>& equity_curve) const {
    if (equity_curve.empty()) {
        throw std::runtime_error("Equity curve cannot be empty for MDD calculation.");
    }

    double max_drawdown = 0.0;
    double peak_equity = equity_curve[0]; // 시작점부터 최고점 추적

    for (double current_equity : equity_curve) {
        // 1. 최고점 갱신
        peak_equity = std::max(peak_equity, current_equity);

        // 2. 현재 하락 폭 계산 (현재 최고점에서 현재 자산 가치까지의 하락)
        double drawdown = (peak_equity - current_equity) / peak_equity;

        // 3. 최대 손실폭 갱신
        max_drawdown = std::max(max_drawdown, drawdown);
    }

    // 결과는 양수(하락폭)로 반환됩니다.
    return max_drawdown;
}
/**
 * @brief 일별 수익률과 무위험 이자율을 기반으로 샤프 비율을 계산합니다.
 * * @param daily_returns 일별 수익률 벡터.
 * @param risk_free_rate_daily 일별 무위험 이자율 (연이율을 거래일 수로 나눈 값).
 * @return double 샤프 비율 값.
 */
double RiskEngine::CalculateSharpe(const std::vector<double>& daily_returns, double risk_free_rate_daily) const {
    if (daily_returns.size() < 2) {
        throw std::runtime_error("Need at least two returns for Sharpe calculation.");
    }

    // 1. 일별 초과 수익률(Excess Return) 계산
    // Excess Return = Daily Return - Risk-Free Rate
    std::vector<double> excess_returns;
    for (double ret : daily_returns) {
        excess_returns.push_back(ret - risk_free_rate_daily);
    }

    size_t N = excess_returns.size();

    // 2. 초과 수익률의 평균 (Average Excess Return) 계산
    double sum_excess_ret = std::accumulate(excess_returns.begin(), excess_returns.end(), 0.0);
    double avg_excess_ret = sum_excess_ret / N;

    // 3. 초과 수익률의 표준 편차 (Volatility) 계산
    double variance_sum = 0.0;
    for (double ex_ret : excess_returns) {
        variance_sum += std::pow(ex_ret - avg_excess_ret, 2);
    }
    // 표본 표준편차를 위해 N-1로 나눔 (N이 클 경우 N으로 나눠도 큰 차이는 없음)
    double daily_std_dev = std::sqrt(variance_sum / (N - 1));

    // 4. 샤프 비율 계산 및 연율화 (Annualization)

    if (daily_std_dev == 0.0) {
        // 변동성이 0인 경우는 이상적인 경우로, 매우 높은 샤프 비율을 반환하거나 예외 처리
        return 100.0;
    }

    // 일별 샤프 비율 = 일별 평균 초과 수익률 / 일별 표준편차
    double daily_sharpe = avg_excess_ret / daily_std_dev;

    // 연율화: 일별 샤프 비율 * sqrt(연간 거래일수) (거래일수 252일 가정)
    double annualized_sharpe = daily_sharpe * std::sqrt(252.0);

    return annualized_sharpe;
}

//PerformanceCalculator
PerformanceMetric PerformanceCalculator::Calculate(const std::vector<TradeRecord>& trade_history,
    const MarketData& data,
    double initial_capital) {

    // 초기화
    daily_equity_.clear();
    daily_returns_.clear();
    trade_pnl_.clear();

    // 임시 포지션 상태 초기화 (PositionManager의 시작 상태와 동일)
    TempPositionState current_pos;
    current_pos.cash = initial_capital;

    int trade_idx = 0;
    int winning_trades = 0;
    int total_closure_trades = 0;
    size_t data_size_t = data.GetDataSize();
    int data_size = static_cast<int>(data_size_t);

    // ----------------------------------------------------
    // 1. 일별 자산 가치 곡선 (Equity Curve) 시뮬레이션
    // ----------------------------------------------------
    for (int day = 0; day < data_size; ++day) {
        const BarData* bar = data.GetBar(day);
        if (!bar) continue;

        // 1a. 해당 날짜에 발생한 모든 거래 처리 (PositionManager 로직 미러링)
        while (trade_idx < trade_history.size() && trade_history[trade_idx].date == bar->date) {
            const TradeRecord& record = trade_history[trade_idx];

            current_pos.cash -= record.fee; // 수수료는 즉시 차감
            double transaction_amount = record.price * record.quantity;

            // BUY (매수) 거래
            if (record.type == "BUY") {
                current_pos.cash -= transaction_amount; // 현금 지출
                current_pos.shares += record.quantity; // 수량 증가

                if (current_pos.shares <= 0) { // 숏 포지션 청산 중 또는 완료
                    // 숏 커버 PnL 계산 (실현 이익: 숏 평균 매도가 - 롱 청산 매입가)
                    int closed_quantity = record.quantity;
                    double realized_pnl = (current_pos.avg_price - record.price) * closed_quantity;
                    trade_pnl_.push_back(realized_pnl);
                    total_closure_trades++;
                    if (realized_pnl > 0) winning_trades++;

                    if (current_pos.shares == 0) current_pos.avg_price = 0.0;

                }
                else { // 롱 포지션 추가 매수
                    double old_value = (current_pos.shares - record.quantity) * current_pos.avg_price;
                    current_pos.avg_price = (old_value + transaction_amount) / current_pos.shares;
                }

            }
            // SELL (매도) 거래
            else if (record.type == "SELL") {
                current_pos.cash += transaction_amount; // 현금 유입
                current_pos.shares -= record.quantity; // 수량 감소

                if (current_pos.shares >= 0) { // 롱 포지션 청산 중 또는 완료
                    // 롱 청산 PnL 계산 (실현 이익: 매도 가격 - 롱 평균 매입가)
                    double realized_pnl = CalculateLongRealizedPnL(current_pos.avg_price, record.price, record.quantity);
                    trade_pnl_.push_back(realized_pnl);
                    total_closure_trades++;
                    if (realized_pnl > 0) winning_trades++;

                    if (current_pos.shares == 0) current_pos.avg_price = 0.0;

                }
                else { // 숏 포지션 진입 또는 추가 매도
                    // 숏 포지션 평균 매도가 갱신
                    int old_abs_shares = std::abs(current_pos.shares + record.quantity);
                    int new_abs_shares = std::abs(current_pos.shares);
                    double old_revenue = old_abs_shares * current_pos.avg_price;

                    double new_total_revenue = old_revenue + transaction_amount;
                    current_pos.avg_price = new_total_revenue / new_abs_shares;
                }
            }

            trade_idx++;
        }

        // 1b. 일별 자산 가치 (Equity) 및 수익률 계산
        double unrealized_value = (double)current_pos.shares * bar->close;
        double current_equity = current_pos.cash + unrealized_value;
        daily_equity_.push_back(current_equity);

        if (day > 0) {
            double prev_equity = daily_equity_[day - 1];
            double daily_ret = (current_equity / prev_equity) - 1.0;
            daily_returns_.push_back(daily_ret);
        }
    }

    // ----------------------------------------------------
    // 2. 최종 성과 지표 계산
    // ----------------------------------------------------
    PerformanceMetric metrics = {};
    if (daily_equity_.empty() || data_size <= 1) return metrics;

    int total_days = data_size;
    double total_trading_years = total_days / 252.0; // 연간 252 거래일 가정

    // A. 누적 수익률 & 연평균 수익률
    double final_equity = daily_equity_.back();
    metrics.cumulative_return = (final_equity / initial_capital) - 1.0;
    // (1 + 누적수익률) ^ (1 / 기간) - 1
    metrics.annualized_return = std::pow(1.0 + metrics.cumulative_return, 1.0 / total_trading_years) - 1.0;

    // B. 총 거래 횟수 & 승률
    metrics.total_trades = trade_history.size();
    metrics.win_rate = (total_closure_trades > 0) ? (double)winning_trades / total_closure_trades : 0.0;

    try {
        metrics.max_drawdown = risk_engine_.CalculateMDD(daily_equity_);
        metrics.sharpe_ratio = risk_engine_.CalculateSharpe(daily_returns_, 0.02 / 252.0); // 무위험 이자율(연 2%) 일별로 가정
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Risk calculation error: " << e.what() << std::endl;
    }

    return metrics;
}

//BacktestController
void BacktestController::SetParameters(int short_ma, int long_ma, double fee_rate, double slippage, double cash, const std::string& data_path) {

    std::cout << "--- 1. SetParameters: Initializing Modules ---" << std::endl;

    // 1. MarketData 로딩
    data_provider_.LoadData(data_path);

    // 2. Strategy 파라미터 설정 및 SignalGenerator에 전략 설정
    // MAStrategy 객체에 기간 설정 (위에서 정의한 헬퍼 메서드 사용)
    ma_strategy_.SetPeriods(short_ma, long_ma);
    signal_generator_.SetStrategy(&ma_strategy_); // MAStrategy 주소를 SignalGenerator에 전달

    // 3. ExecutionModel 파라미터 설정
    execution_model_.SetParameters(fee_rate, slippage,cash);

    // 4. PositionManager 초기화
    position_manager_.Reset(cash);

    // 5. 이전 결과 초기화
    final_metrics_ = {};

    std::cout << "Initialization complete. Ready to run backtest." << std::endl;
}
void BacktestController::RunBacktest() {

    std::cout << "--- 2. RunBacktest: Starting Core Loop ---" << std::endl;

    int data_size = data_provider_.GetDataSize();
    if (data_size == 0) {
        std::cerr << "Error: No data loaded. Cannot run backtest." << std::endl;
        return;
    }

    // 1. 지표(Indicator) 계산
    std::map<std::string, std::vector<double>> indicators;

    // 단기 MA 계산
    indicators["MA_" + std::to_string(ma_strategy_.GetShortPeriod())] =
        indicator_engine_.CalculateMA(data_provider_, ma_strategy_.GetShortPeriod());

    // 장기 MA 계산
    indicators["MA_" + std::to_string(ma_strategy_.GetLongPeriod())] =
        indicator_engine_.CalculateMA(data_provider_, ma_strategy_.GetLongPeriod());

    // 2. 시그널 생성 (전략 패턴 사용)
    std::vector<int> signals = signal_generator_.GetSignals(data_provider_, indicators);
    if (signals.size() != data_size) {
        std::cerr << "Error: Signal size mismatch with data size." << std::endl;
        return;
    }

    // 3. 백테스트 메인 루프 (날짜별 시뮬레이션)
    for (int i = 0; i < data_size; ++i) {
        const BarData* current_bar = data_provider_.GetBar(i);
        int current_signal = signals[i];

        // 현재 포지션 정보 (ExecutionModel에 전달)
        double holding_price = position_manager_.GetAveragePrice();
        int shares_held = position_manager_.GetSharesHeld();

        // 3a. Execution: 시그널에 따른 실제 거래 명령 생성
        TradeRecord record = execution_model_.ExecuteOrder(
            current_signal,
            *current_bar,
            holding_price,
            shares_held
        );

        // 3b. Position: 거래 기록에 따른 포지션 갱신 (현금, 수량, 평단가)
        position_manager_.UpdatePosition(record);
    }

    //마지막 날 포지션이 남아있다면 강제 청산
    if (position_manager_.GetSharesHeld() > 0) {
        const BarData* last_bar = data_provider_.GetBar(data_size - 1);
        TradeRecord close_record;
        close_record.date = last_bar->date;
        close_record.type = "SELL";
        close_record.price = last_bar->close; // 종가로 청산
        close_record.quantity = position_manager_.GetSharesHeld();
        close_record.fee = (close_record.price * close_record.quantity) * 0.001; // 수수료 등

        position_manager_.UpdatePosition(close_record);
    }
    // 4. 성과 계산 및 저장
    std::cout << "--- 3. Calculate Performance ---" << std::endl;

    const std::vector<TradeRecord>& history = position_manager_.GetTradeHistory();
    double initial_capital = 10000000.0; // PositionManager의 초기 자본과 일치해야 함

    final_metrics_ = performance_calculator_.Calculate(
        history,
        data_provider_,
        initial_capital
    );

    std::cout << "Backtest finished. Total trades: " << history.size()
        << ", Final Return: " << final_metrics_.cumulative_return * 100.0 << "%" << std::endl;
}