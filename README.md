# nodemcu32s-idf-playground
這是一個基於 **ESP-IDF** 框架的 NodeMCU-32S (ESP32) 專案。本專案實作了多種硬體周邊控制，包含 GPIO、NVS、UART、SPI 顯示器及 WiFi 應用，大部分範例參考並實作自 [Luca Dentella 的教學系列](https://www.lucadentella.it/en/category/tutorial/)。

## 目錄結構 (Folder Structure)
```text
nodemcu32s-idf-playground/
├── main/
│   ├── CMakeLists.txt      # 定義編譯 main.c
│   └── main.c              # 使用者在此貼上程式碼進行編譯
├── examples/               # 各類測試功能範例檔
│   ├── 01_GPIO/            # 基礎 I/O、按鈕中斷 (ISR)、七段顯示器
│   ├── 02_NVS/             # 非揮發性儲存讀寫
│   ├── 03_UART/            # 序列通訊測試
│   ├── 04_SPI/             # ILI9341 螢幕驅動與圖片顯示
│   └── 05_WIFI/            # WiFi 掃描、連線、SNTP 對時、HTTP 客戶端
├── CMakeLists.txt          # 專案根目錄編譯設定
├── partitions.csv          # 自定義 Flash 分區表 (支援 NVS 與 SPI 儲存)
└── README.md

```

## 硬體需求 (Hardware Requirements)
* **開發板**: NodeMCU-32S (ESP32-WROOM-32)
* **GPIO 測試**: 按鈕 (Button)、七段顯示器 (Segment)
* **SPI 測試**: ILI9341 TFT 顯示器
* **其他**: 麵包板、跳線、適當的電阻


## 快速上手 (How to Run)
### 1. 環境準備 (Prerequisites)
請先安裝 **ESP-IDF v5.x** 工具鏈  
[ESP-IDF v5.* Windows Installer Download](https://dl.espressif.com/dl/esp-idf/)

```bash
# 設定環境變數 (路徑請依實際安裝位置修改)
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.2
%IDF_PATH%\export.bat

# 確認版本
idf.py --version 
```

### 2. 選擇並執行範例
本專案採用「程式碼替換法」進行獨立測試：

1. 進入 `examples/` 資料夾，選取想測試的 `.c` 檔案。
2. **複製**該檔案內容，**貼上並覆蓋**到 `main/main.c`。
3. **注意**：若執行 **04_SPI** 範例，請將資料夾內的 `.h` 標頭檔一併複製到 `main/` 目錄。
4. 開啟終端機執行編譯與燒錄：

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM10 flash monitor  # 請將 COM10 改為你的序列埠
```

* **燒錄提示**：看到 `Connecting........` 時，請長按板子上的 **IO0 (BOOT)** 按鈕直到開始寫入。
* **退出監控**：在 Monitor 畫面按下 `Ctrl + ]`。

## 接腳對照 (Pin Mapping)
| 功能 (Module)    | 腳位 (GPIO) | 備註 (Notes)    |
| ---------------- | ---------- | --------------- |
| **LED (Simple)** | GPIO 2     | 板載 LED        |
| **Button**       | GPIO 4     | 需下拉或上拉電阻 |
| **SPI MOSI**     | GPIO 23    | 連接 ILI9341    |
| **SPI CLK**      | GPIO 18    | 連接 ILI9341    |
| **UART TX/RX**   | GPIO 1/3   | 預設監控使用     |

## 技術要點 (Technical Notes)
* **Partition Table**: 本專案使用自定義 `partitions.csv`。請確保在 `menuconfig` 中將 `Partition Table` 設定為 **Custom**，以支援 NVS 儲存。
* **WiFi 設定**: 使用 WiFi 範例前，請直接在 `main.c` 中修改 SSID 與密碼。


## 專案結果 (Project Results)
[![專案演示影片](https://img.youtube.com/vi/Cg3L2-22ZxM/hqdefault.jpg)](https://youtube.com/shorts/Cg3L2-22ZxM)

點擊上方圖片跳轉至 YouTube 觀看實作演示

## 參考資源 (References)
* [Luca Dentella's ESP32 Tutorials](https://www.lucadentella.it/en/category/tutorial/)
* [NodeMCU-32S Datasheet](http://robu.in/wp-content/uploads/2017/10/SKU-44682-datasheet.pdf)
* [NodeMCU-32S Pin Mapping](https://mischianti.org/wp-content/uploads/2024/02/ESP32-NODEMCU-ESP-32S-Kit-pinout-low-res-mischianti-1024x599.jpg.webp)
* [ILI9341 Datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)