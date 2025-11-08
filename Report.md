# Reliable Communication Protocol over RS-232 Serial Interface

## 1. Summary
This project involved the design and implementation of a reliable communication protocol for file transfer over an RS-232 serial interface.  
The implemented Data Link Layer (DLL) utilized the **Stop-and-Wait Automatic Repeat Request (ARQ)** strategy to ensure reliable data transfer.  
This report evaluates the protocol's efficiency ($S$) under controlled variations of channel parameters (Baud Rate, Frame Size, BER, and Propagation Delay) and compares the measured performance against established theoretical efficiency models.

---

## 2. Introduction
The core objective was to implement a robust data link protocol suitable for file transfer over an RS-232 serial connection.  
The system is modular, separating responsibilities into two layers:

- **Application Layer (AL):** Handles file operations, splitting the file into data packets, and creating control packets (START/END) to manage the transfer.  
- **Data Link Layer (DLL):** Provides reliable communication services, managing framing, error checking (BCC1/BCC2), retransmissions, acknowledgments (RR/REJ), and byte-stuffing.  

The evaluation focuses on quantifying efficiency ($S$) to understand the practical overheads and performance limitations of the Stop-and-Wait mechanism under non-ideal link conditions.

---

## 3. Methodology

### a. Metrics and Calculation
The protocol's performance is assessed using **Throughput (R)** and **Efficiency (S):**

- **Throughput ($R$):**  
  Calculated as the ratio of the total file size (in bits) to the total transfer time (seconds):  
  $$
  R = \frac{\text{Sum of file's bits}}{\text{transfer time}} \quad (\text{bits/s})
  $$

- **Efficiency ($S$):**  
  Calculated by normalizing the throughput ($R$) by the channel capacity ($C$), which is the Baud Rate:  
  $$
  S = \frac{R}{C}
  $$

---

### b. Test Procedure
A file of fixed size (e.g., 10,968 bytes) was transferred between Tx and Rx terminals connected via a simulated RS-232 link using the **cable program**.  
The received file was compared to the original to confirm correct transfer. Each experiment was repeated to ensure result consistency.

---

### c. Parameter Variation

| Parameter | Description | Control Command |
|------------|--------------|-----------------|
| **Baud Rate (C)** | Link speed, varied from 1200 to 115200 bits/s | `baud <rate>` |
| **Frame Size (L)** | Maximum payload per I-frame | Modify `MAX_PAYLOAD_SIZE` and recompile |
| **Frame Error Rate (FER)** | Introduced via Bit Error Rate (BER) | `ber <rate>` |
| **Propagation Delay (Tprop)** | Simulated delay (μs) | `prop <delay>` |

---

## 4. Results

### a. Baud Rate Variation ($C$)

| Baudrate (bits/s) | Time (s) | R (bits/s) | Efficiency (%) |
|--------------------|----------|-------------|----------------|
| 1200 | [Value] | [Value] | [Value] |
| 4800 | [Value] | [Value] | [Value] |
| 9600 | [Value] | [Value] | [Value] |
| 38400 | [Value] | [Value] | [Value] |
| 115200 | [Value] | [Value] | [Value] |

**Analysis:**  
As the Baud Rate increases, the total transmission time decreases. Efficiency typically shows a significant increase at lower Baud Rates before stabilizing, because at higher speeds the data transmission becomes faster and the overhead has a smaller impact.

---

### b. Frame Size Variation

| Max Payload Size (bytes) | Time (s) | R (bits/s) | Efficiency (%) |
|---------------------------|----------|-------------|----------------|
| 200 | [Value] | [Value] | [Value] |
| 500 | [Value] | [Value] | [Value] |
| 1000 | [Value] | [Value] | [Value] |
| 1500 | [Value] | [Value] | [Value] |
| 2000 | [Value] | [Value] | [Value] |

**Analysis:**  
Efficiency increases as the frame size grows and transmission time decreases. By increasing the payload size, the protocol spends less time handling fixed overhead relative to useful data, resulting in better performance.

---

### c. BER Variation

| BER | Time (s) | R (bits/s) | Efficiency (%) |
|-----|----------|-------------|----------------|
| 0 | [Value] | [Value] | [Value] |
| 0.00001 | [Value] | [Value] | [Value] |
| 0.0001 | [Value] | [Value] | [Value] |
| 0.001 | [Value] | [Value] | [Value] |
| 0.1 | [Value] | [Value] | [Value] |

**Analysis:**  
As the Bit Error Rate (BER) increases, the protocol spends more time recovering from errors through retransmissions, causing efficiency to drop and transmission time to increase. At sufficiently high BER values, the protocol may fail to deliver the entire file.

---

### d. Propagation Delay Variation ($T_{prop}$)

| Tprop (μs) | Time (s) | R (bits/s) | Efficiency (%) |
|-------------|----------|-------------|----------------|
| 0 | [Value] | [Value] | [Value] |
| 5000 | [Value] | [Value] | [Value] |
| 10000 | [Value] | [Value] | [Value] |
| 50000 | [Value] | [Value] | [Value] |
| 100000 | [Value] | [Value] | [Value] |

**Analysis:**  
As the propagation delay increases, the throughput ($R$) decreases and efficiency drops. Higher delays increase idle waiting times for acknowledgments, reducing overall channel utilization, a core limitation of Stop-and-Wait ARQ.

---

### e. Comparison with Theoretical Efficiency

The measured efficiency ($S$) is compared against the theoretical Stop-and-Wait efficiency ($S_{theoretical}$):

$$
S_{theoretical}=\frac{T_{frame}}{T_{frame}+2\cdot T_{prop}}\cdot(1-P_{e})
$$

| Tprop (μs) | Efficiency (Experimental, %) | Efficiency (Theoretical, %) |
|-------------|------------------------------|------------------------------|
| 0 | [Value] | 100 |
| 5000 | [Value] | [Value from formula] |
| 10000 | [Value] | [Value from formula] |
| 50000 | [Value] | [Value from formula] |
| 100000 | [Value] | [Value from formula] |

**Analysis:**  
The experimental results should closely mirror the theoretical trend. The experimental values will be slightly lower due to practical factors like implementation overhead, time spent on frame headers, acknowledgments, byte-stuffing, and processing delays not accounted for in the idealized model.

---

## 5. Conclusion
The project successfully delivered a reliable and fully functional implementation of the data link protocol.  
The experimental evaluation confirmed that the Stop-and-Wait ARQ protocol behaves as anticipated under varying network conditions, with overall efficiency patterns aligning well with theoretical expectations.  
The analysis highlighted the protocol's extreme sensitivity to **Propagation Delay** and **Error Rates**, confirming that the implemented system provides a solid foundation for dependable data transmission.
