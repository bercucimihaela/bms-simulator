## BMS Simulator - Li-Ion Battery Pack

Simulator in C++17 al unui sistem de management al bateriilor (BMS) pentru un pack Li-Ion. Monitorizeaza tensiunea celulelor, curentul si temperatura, si ia decizii active folosind o masina de stari.

---

## Senzorii simulati

| Senzor | Valoare normala | Alerta daca |
|---|---|---|
| CellVoltage | 4.2V → 3.0V | < 2.8V |
| PackCurrent | ~15A | > 45A |
| BatteryTemp | 25°C | > 70°C |

Programul ruleaza 100 timesteps, logheaza fiecare citire si raporteaza tranzitiile de stare.

---

## State Machine

- `NORMAL` — toti parametrii in limite
- `THERMAL_THROTTLING` — temp > 55°C sau curent > 40A → reducere curent maxim
- `CRITICAL_DISCONNECT` — tensiune < 2.8V sau temp > 70°C → deconectare contactori

---

## Design OOP

- `BmsSensor` clasa abstracta cu `read()` si `getUnit()`
- `CellVoltageSensor`, `ShuntCurrentSensor`, `NtcThermistor` derivate
- `DataLogger` tine istoricul citirilor
- `BmsController` contine totul, ruleaza simularea

---

## Build & run

```
mkdir build
cd build
cmake ..
cmake --build .
./BmsSimulator
```

Sau direct in CLion.

---

## Output

<img width="768" height="337" alt="bms_output" src="https://github.com/user-attachments/assets/2ac1e7ad-df34-49be-9f74-610f97e6af48" />

Consola demonstrează rularea simulării pe durata a 100 de pași (timesteps). Se poate observa cum BMS-ul monitorizează parametrii și reacționează activ: comută din starea NORMAL în THERMAL_THROTTLING pentru a reduce curentul, iar la depășirea pragului termic de 70°C, declanșează imediat starea de protecție CRITICAL_DISCONNECT
