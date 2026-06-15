// Battery Management System (BMS) Simulator
// Simuleaza un BMS pentru un pack de baterii Li-Ion.
// Sistemul monitorizeaza tensiunea celulelor, curentul si temperatura,
// si ia decizii active: throttling termic sau deconectare critica.

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <algorithm>

// ─────────────────────────────────────────────
// Clasa abstracta - orice senzor din BMS
// ─────────────────────────────────────────────
class BmsSensor {
protected:
    std::string name;
    double currentValue;
    double minThreshold;
    double maxThreshold;

public:
    BmsSensor(const std::string& sensorName, double minVal, double maxVal)
        : name(sensorName), currentValue(0.0),
          minThreshold(minVal), maxThreshold(maxVal) {}

    virtual ~BmsSensor() = default;

    virtual double read() = 0;
    virtual std::string getUnit() const = 0;

    bool isOutOfRange() const {
        return (currentValue < minThreshold || currentValue > maxThreshold);
    }

    std::string getName()  const { return name; }
    double getValue()      const { return currentValue; }
    double getMin()        const { return minThreshold; }
    double getMax()        const { return maxThreshold; }
};

// ─────────────────────────────────────────────
// Senzor tensiune celula Li-Ion
// Simuleaza descarcarea treptata de la 4.2V spre 3.0V
// ─────────────────────────────────────────────
class CellVoltageSensor : public BmsSensor {
private:
    double voltage;

public:
    CellVoltageSensor(const std::string& name, double startVoltage, double minV, double maxV)
        : BmsSensor(name, minV, maxV), voltage(startVoltage) {}

    double read() override {
        // descarcarea treptata + ripple electric mic
        voltage -= 0.01 + ((std::rand() % 10) / 1000.0);
        double ripple = ((std::rand() % 21) - 10) / 1000.0;
        currentValue = voltage + ripple;
        if (currentValue < 2.5) currentValue = 2.5; // limita fizica celula
        return currentValue;
    }

    std::string getUnit() const override { return "V"; }
};

// ─────────────────────────────────────────────
// Senzor curent shunt
// Simuleaza curentul tras din pack - cu fluctuatii bruste (accelerare motor)
// ─────────────────────────────────────────────
class ShuntCurrentSensor : public BmsSensor {
private:
    double baseCurrent;

public:
    ShuntCurrentSensor(const std::string& name, double base, double minA, double maxA)
        : BmsSensor(name, minA, maxA), baseCurrent(base) {}

    double read() override {
        // fluctuatie brusca ocazionala (pornire motor, accelerare)
        double spike = 0.0;
        if (std::rand() % 10 == 0) {
            spike = ((std::rand() % 30) + 10); // spike intre 10-40A
        }
        double noise = ((std::rand() % 21) - 10) / 10.0;
        currentValue = baseCurrent + noise + spike;
        if (currentValue < 0.0) currentValue = 0.0;
        return currentValue;
    }

    std::string getUnit() const override { return "A"; }
};

// ─────────────────────────────────────────────
// NTC Thermistor - temperatura pack baterie
// Creste cand curentul tras este mare
// ─────────────────────────────────────────────
class NtcThermistor : public BmsSensor {
private:
    double temperature;
    const ShuntCurrentSensor& currentRef;

public:
    NtcThermistor(const std::string& name, double startTemp,
                  double minC, double maxC,
                  const ShuntCurrentSensor& currentSensor)
        : BmsSensor(name, minC, maxC),
          temperature(startTemp),
          currentRef(currentSensor) {}

    double read() override {
        // temperatura creste proportional cu curentul (efect Joule simplificat)
        double current = currentRef.getValue();
        double heating = current * 0.05;
        double cooling = 0.3; // racire pasiva
        temperature += heating - cooling;
        if (temperature < 20.0) temperature = 20.0;
        double noise = ((std::rand() % 11) - 5) / 10.0;
        currentValue = temperature + noise;
        return currentValue;
    }

    std::string getUnit() const override { return "C"; }
};

// ─────────────────────────────────────────────
// Logger de date pentru fiecare senzor
// ─────────────────────────────────────────────
class DataLogger {
private:
    std::string sensorName;
    std::vector<double> history;
    int maxEntries;

public:
    DataLogger(const std::string& name, int maxSize = 200)
        : sensorName(name), maxEntries(maxSize) {}

    void log(double value) {
        if ((int)history.size() >= maxEntries)
            history.erase(history.begin());
        history.push_back(value);
    }

    double getAverage() const {
        if (history.empty()) return 0.0;
        double sum = 0.0;
        for (double v : history) sum += v;
        return sum / history.size();
    }

    double getMax() const {
        if (history.empty()) return 0.0;
        return *std::max_element(history.begin(), history.end());
    }

    double getMin() const {
        if (history.empty()) return 0.0;
        return *std::min_element(history.begin(), history.end());
    }

    int getCount() const { return (int)history.size(); }
};

// ─────────────────────────────────────────────
// Starile sistemului BMS (State Machine)
// ─────────────────────────────────────────────
enum class BmsState {
    NORMAL,
    THERMAL_THROTTLING,
    CRITICAL_DISCONNECT
};

std::string stateToString(BmsState state) {
    switch (state) {
        case BmsState::NORMAL:               return "NORMAL";
        case BmsState::THERMAL_THROTTLING:   return "THERMAL_THROTTLING";
        case BmsState::CRITICAL_DISCONNECT:  return "CRITICAL_DISCONNECT";
        default:                             return "UNKNOWN";
    }
}

// ─────────────────────────────────────────────
// BmsController - creierul sistemului
// Citeste senzorii, ia decizii, schimba starea
// ─────────────────────────────────────────────
class BmsController {
private:
    ShuntCurrentSensor* currentSensor;
    CellVoltageSensor*  voltageSensor;
    NtcThermistor*      tempSensor;

    DataLogger loggerCurrent;
    DataLogger loggerVoltage;
    DataLogger loggerTemp;

    BmsState state;
    int currentStep;
    int totalEvents;

    struct Event {
        int step;
        std::string type;
        double value;
        std::string action;
    };
    std::vector<Event> eventLog;

    void evaluate() {
        double temp    = tempSensor->getValue();
        double voltage = voltageSensor->getValue();
        double current = currentSensor->getValue();

        BmsState newState = state;

        // Logica State Machine
        if (voltage < 2.8 || temp > 70.0) {
            newState = BmsState::CRITICAL_DISCONNECT;
        } else if (temp > 55.0 || current > 40.0) {
            newState = BmsState::THERMAL_THROTTLING;
        } else {
            newState = BmsState::NORMAL;
        }

        // Detecteaza tranzitii de stare
        if (newState != state) {
            std::string action;
            if (newState == BmsState::THERMAL_THROTTLING)
                action = "Reducere curent maxim la 20A";
            else if (newState == BmsState::CRITICAL_DISCONNECT)
                action = "Deconectare contactori - protectie pack";
            else
                action = "Revenire la operare normala";

            std::string trigger;
            if (temp > 70.0)        trigger = "Temp=" + std::to_string((int)temp) + "C";
            else if (voltage < 2.8) trigger = "Vcell=" + std::to_string(voltage).substr(0,4) + "V";
            else if (temp > 55.0)   trigger = "Temp=" + std::to_string((int)temp) + "C";
            else if (current > 40.0) trigger = "I=" + std::to_string((int)current) + "A";
            else                    trigger = "Parametri normali";

            eventLog.push_back({currentStep, stateToString(newState), 0.0, action});
            totalEvents++;

            std::cout << "  [STATE] Step " << std::setw(3) << currentStep
                      << " | " << std::left << std::setw(22) << stateToString(newState)
                      << " | Trigger: " << trigger
                      << " | " << action << "\n";

            state = newState;
        }
    }

public:
    BmsController()
        : loggerCurrent("Current"), loggerVoltage("Voltage"), loggerTemp("Temperature"),
          state(BmsState::NORMAL), currentStep(0), totalEvents(0) {

        currentSensor = new ShuntCurrentSensor("PackCurrent", 15.0, 0.0, 45.0);
        voltageSensor = new CellVoltageSensor("CellVoltage", 4.2, 2.8, 4.25);
        tempSensor    = new NtcThermistor("BatteryTemp", 25.0, 0.0, 70.0, *currentSensor);
    }

    ~BmsController() {
        delete currentSensor;
        delete voltageSensor;
        delete tempSensor;
    }

    void tick() {
        currentStep++;
        currentSensor->read();
        voltageSensor->read();
        tempSensor->read();

        loggerCurrent.log(currentSensor->getValue());
        loggerVoltage.log(voltageSensor->getValue());
        loggerTemp.log(tempSensor->getValue());

        if (state == BmsState::CRITICAL_DISCONNECT) return;

        evaluate();
    }

    void run(int steps) {
        std::cout << "=== BMS SIMULATOR - Li-Ion Pack ===\n";
        std::cout << "Simulare " << steps << " timesteps (ex: " << steps * 100 << "ms total)\n\n";

        for (int i = 0; i < steps; i++) {
            tick();
            if (state == BmsState::CRITICAL_DISCONNECT) {
                std::cout << "  [BMS] Pack deconectat la step " << currentStep << ". Simulare oprita.\n";
                break;
            }
        }
    }

    void printStatistics() const {
        std::cout << "\n=== STATISTICI PACK ===\n";
        std::cout << std::left
                  << std::setw(18) << "Parametru"
                  << std::setw(10) << "Citiri"
                  << std::setw(10) << "Medie"
                  << std::setw(10) << "Min"
                  << std::setw(10) << "Max"
                  << "Limite\n";
        std::cout << std::string(68, '-') << "\n";

        auto printRow = [](const std::string& name, const DataLogger& log,
                           const BmsSensor* s) {
            std::cout << std::left
                      << std::setw(18) << name
                      << std::setw(10) << log.getCount()
                      << std::fixed << std::setprecision(2)
                      << std::setw(10) << log.getAverage()
                      << std::setw(10) << log.getMin()
                      << std::setw(10) << log.getMax()
                      << "[" << s->getMin() << " - " << s->getMax()
                      << " " << s->getUnit() << "]\n";
        };

        printRow("PackCurrent (A)",  loggerCurrent, currentSensor);
        printRow("CellVoltage (V)",  loggerVoltage, voltageSensor);
        printRow("BatteryTemp (C)",  loggerTemp,    tempSensor);
    }

    void printEventLog() const {
        std::cout << "\n=== EVENT LOG ===\n";
        std::cout << "Total tranzitii de stare: " << totalEvents << "\n";
        if (eventLog.empty()) {
            std::cout << "  Nicio tranzitie detectata.\n";
            return;
        }
        for (const auto& e : eventLog) {
            std::cout << "  Step " << std::setw(3) << e.step
                      << " | " << std::left << std::setw(22) << e.type
                      << " | " << e.action << "\n";
        }
        std::cout << "\nStare finala: " << stateToString(state) << "\n";
    }
};

// ─────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────
int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    BmsController bms;
    bms.run(100);
    bms.printStatistics();
    bms.printEventLog();

    return 0;
}
