#include <iostream>
#include <vector>
#include <string>
#include <map>

//==============================================================
// 1. 공통 구조체 (Data Transfer Objects)
//==============================================================

struct TradeRecord {
    std::string date = "NULL";
    std::string type = "NULL";// "BUY" or "SELL"
    double price = 0.0;
    int quantity = 0;
    double fee = 0.0; //수수료
};

struct PerformanceMetric { //성과지표들
    double cumulative_return = 0.0; //누적수익률(백테스트 전체기간 수익률)
    double annualized_return = 0.0; //연평균 수익률
    double max_drawdown = 0.0; //최대손실폭(MDD)
    double sharpe_ratio = 0.0; //샤프 비율(위험 대비 초과 수익률)
    double win_rate = 0.0; //승률
    int total_trades = 0; //총 거래 횟수(매수 및 매도 이벤트의 총합)
};

struct BarData {
    std::string date;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    int volume = 0;
    int signal = 0;
};

struct SignalData {
    std::string date; // 신호가 발생한 날짜 (YYYY-MM-DD 형식)
    int signal = 0;   // 신호 값 (1: 매수, -1: 매도, 0: 관망/유지)
};

//==============================================================
// 2. 데이터 & 지표 계층 (Data & Indicator Layer)
//==============================================================

class MarketData {
public:
    MarketData() = default;

    // 메서드
    void LoadData(const std::string& file_path);
    const BarData* GetBar(int index) const;
    int GetDataSize() const { return historical_data_.size(); }
    const std::vector<BarData>& GetHistoricalData() const { return historical_data_; }

private:
    std::vector<BarData> historical_data_;
};

class IndicatorEngine {
public:
    IndicatorEngine() = default;

    // 메서드
    std::vector<double> CalculateMA(const MarketData& data, int period);
    //std::vector<double> CalculateRSI(const MarketData& data, int period); // 확장성 고려
    // ... 다른 지표 계산 메서드 선언
};

//==============================================================
// 3. 전략 & 실행 계층 (Strategy & Execution Layer)
//==============================================================

// Strategy Base Class (다형성 확보를 위한 가상 클래스)
class Strategy {
public:
    virtual ~Strategy() = default;

    // 메서드
    // Signal: -1(Sell), 0(Hold), 1(Buy)
    virtual std::vector<int> GenerateSignal(const MarketData& data,
        const std::map<std::string, std::vector<double>>& indicators) = 0;
};

class MAStrategy : public Strategy {
public:
    MAStrategy(int short_period, int long_period)
        : short_period_(short_period), long_period_(long_period) {
    }

    // 메서드
    std::vector<int> GenerateSignal(const MarketData& data,
        const std::map<std::string, std::vector<double>>& indicators) override;

    void SetPeriods(int short_period, int long_period) {
        short_period_ = short_period;
        long_period_ = long_period;
    }
    int GetShortPeriod() const { return short_period_; }
    int GetLongPeriod() const { return long_period_; }

private:
    int short_period_ = 0;
    int long_period_ = 0;
};

class SignalGenerator {
public:
    SignalGenerator() = default;

    // 메서드
    void SetStrategy(Strategy* strategy) { current_strategy_ = strategy; }
    std::vector<int> GetSignals(const MarketData& data,
        const std::map<std::string, std::vector<double>>& indicators);

    // Python으로 신호 데이터를 전달할 getter
    const std::vector<SignalData>& GetSignalHistory() const { return signal_history_; }

    // 재실행을 위한 리셋
    void Reset() { signal_history_.clear(); }

private:
    Strategy* current_strategy_ = nullptr;
    std::vector<SignalData> signal_history_;
};

class ExecutionModel {
public:
    ExecutionModel(double fee_rate, double slippage) : fee_rate_(fee_rate), slippage_(slippage) {};

    // 메서드
    // 시그널과 현재 포지션을 고려하여 실제 TradeRecord를 생성
    TradeRecord ExecuteOrder(int signal, const BarData& current_bar,
        double current_holding_price, int current_shares_held);

    void SetParameters(double fee_rate, double slippage , double cash) {
        fee_rate_ = fee_rate;
        slippage_ = slippage;
        cash_ = cash;
    }

private:
    double fee_rate_;
    double slippage_;
    double cash_;
};

//==============================================================
// 4. 관리 & 성과 계층 (Management & Performance Layer)
//==============================================================

class PositionManager {
public:
    PositionManager() = default;

    // 메서드
    void UpdatePosition(const TradeRecord& record);
    double GetAveragePrice() const { return average_price_; }
    int GetSharesHeld() const { return shares_held_; }
    std::string GetPositionStatus() const { return position_status_; }
    const std::vector<TradeRecord>& GetTradeHistory() const { return trade_history_; }
    void Reset(double cash) {
        position_status_ = "NONE";
        shares_held_ = 0;
        average_price_ = 0.0;
        cash_balance_ = cash; // 초기 현금 가정
        trade_history_.clear();
    }

private:
    std::string position_status_ = "NONE";
    int shares_held_ = 0;
    double average_price_ = 0.0;
    double cash_balance_ = 0; // 초기 현금 가정
    std::vector<TradeRecord> trade_history_;
};

class RiskEngine {
public:
    RiskEngine() = default;

    // 메서드
    // MDD, 변동성 등 리스크 관련 지표만 계산
    double CalculateMDD(const std::vector<double>& equity_curve) const;
    double CalculateSharpe(const std::vector<double>& returns, double risk_free_rate = 0.0) const;
};

class PerformanceCalculator {
public:
    PerformanceCalculator() = default;

    // 메서드
    PerformanceMetric Calculate(const std::vector<TradeRecord>& trade_history,
        const MarketData& data, double initial_capital);

    // 롱 포지션 청산 시 PnL을 계산하는 헬퍼 함수
    double CalculateLongRealizedPnL(double avg_entry_price, double exit_price, int quantity) const {
        // PnL = (Exit Price - Entry Price) * Quantity
        return (exit_price - avg_entry_price) * quantity;
    }

private:
    std::vector<double> daily_equity_; // 일별 자산 가치 곡선
    std::vector<double> daily_returns_; // 일별 수익률
    std::vector<double> trade_pnl_; // 실현 손익 기록
    RiskEngine risk_engine_;

    //PositionManager의 로직을 PerformanceCalculator 내에서 재현하기 위한 임시 구조체
    struct TempPositionState {
        double cash = 0.0;
        int shares = 0;
        double avg_price = 0.0; // 롱: 평균 매입가, 숏: 평균 매도가
    };
};

//==============================================================
// 5. 총괄 & API 계층 (Controller & Facade Layer)
//==============================================================

class BacktestController {
public:
    BacktestController() = default;

    // 메서드
    void SetParameters(int short_ma, int long_ma, double fee_rate, double slippage, double cash, const std::string& data_path);
    void RunBacktest();
    PerformanceMetric GetPerformanceMetrics() const { return final_metrics_; }
    const std::vector<SignalData>& GetChartData() const { return signal_generator_.GetSignalHistory(); } // 차트 표시용 데이터

private:
    MarketData data_provider_;
    IndicatorEngine indicator_engine_;
    MAStrategy ma_strategy_{ 0, 0 }; // 초기화는 SetParameters에서
    SignalGenerator signal_generator_;
    ExecutionModel execution_model_{ 0.0, 0.0 }; // 초기화는 SetParameters에서
    PositionManager position_manager_;
    PerformanceCalculator performance_calculator_;
    RiskEngine risk_engine_;

    PerformanceMetric final_metrics_;
};

// StrategyFacade (pybind11을 통해 Python으로 노출될 단일 API 클래스)／－
class StrategyFacade {
public:
    StrategyFacade() = default;

    // 메서드
    // Python에서 전달된 파라미터로 백테스트 실행을 요청
    void RunBacktest(int short_ma, int long_ma, double fee_rate, double slippage, double cash, const std::string& data_path) {
        controller_.SetParameters(short_ma, long_ma, fee_rate, slippage, cash,data_path);
        controller_.RunBacktest();
    }
    // Python으로 최종 결과 구조체를 반환
    PerformanceMetric GetResults() const { return controller_.GetPerformanceMetrics(); }
    // Python으로 차트 데이터를 반환
    std::vector<SignalData> GetChartData() const { return controller_.GetChartData(); }

private:
    BacktestController controller_;
};