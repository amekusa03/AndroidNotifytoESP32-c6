# システム概要書: Android & Linux 通知連携 / スマートホーム電源制御 / 非常停止システム

Waveshare **ESP32-C6-GEEK** を中核とし、Android・Linux の通知表示、スマートホーム（Matter）による Linux PC の電源制御、および物理非常停止ボタンによる PC スクリーンロックを実現する統合システムの技術・運用仕様書です。

---

## 📐 システム全体アーキテクチャ

```mermaid
graph TD
    subgraph Mobile["Android 端末"]
        A_App["Android 通知転送アプリ<br/>(NotificationListenerService)"]
    end

    subgraph LinuxPC["Linux / Ubuntu PC"]
        L_Dbus["D-Bus 通知システム"]
        L_Tray["Qt常駐アプリ / Pythonスクリプト<br/>(ubuntu_notifier)"]
        L_Daemon["PC制御デーモン<br/>(ubuntu_shutdown_daemon.py:7777)"]
        L_Screen["セッションロック<br/>(loginctl / GNOME / KDE)"]
        
        L_Dbus --> L_Tray
        L_Daemon -->|画面ロック / 解除| L_Screen
        L_Daemon -->|シャットダウン| LinuxPC
    end

    subgraph SmartHome["スマートホーム (Matter)"]
        HomeHub["Google Home / Apple Home"]
    end

    subgraph ESP32["ESP32-C6-GEEK ドングル (USB直結)"]
        subgraph Hardware["ハードウェア"]
            LCD["1.14\" ST7789 LCD<br/>(240x135)"]
            LED["通知LED<br/>(GPIO 7)"]
            Btn["非常停止ボタン (B接点 NC)<br/>(GPIO 8)"]
            USB["USB-CDC / JTAG 接続"]
        end

        subgraph Firmware["ESP-IDF ファームウェア"]
            NotifySrv["通知受信サーバー<br/>(TCP 5555 / mDNS)"]
            MatterDev["Matter Dimmable Plug-in Unit"]
            PCMon["PC状態監視<br/>(USB接続ポーリング)"]
            EStopMon["非常停止ボタン監視タスク"]
        end
    end

    %% データフロー
    A_App -->|Wi-Fi: TCP 5555<br/>RGB565画像転送| NotifySrv
    L_Tray -->|Wi-Fi / LAN: TCP 5555<br/>RGB565画像転送| NotifySrv
    NotifySrv -->|通知画像描画| LCD
    NotifySrv -->|表示中 HIGH 点灯| LED

    HomeHub <-->|Matter Wi-Fi| MatterDev
    MatterDev -->|電源ON: WOL マジックパケット| LinuxPC
    MatterDev -->|電源OFF / 音量: 認証トークン| L_Daemon
    PCMon <-->|USB接続状態検知 (2秒周期)| USB
    PCMon -.->|Matter属性自動同期| MatterDev

    Btn -->|押下 HIGH 検知| EStopMon
    EStopMon -->|非常停止トークン| L_Daemon
```

---

## 🌟 主要機能の詳細仕様

### 1. Android & Linux 通知の LCD 表示および LED 点灯

Android スマートフォンおよび Linux デスクトップの通知をリアルタイムに ESP32 へ転送し、視覚的に通知します。

- **通知キャッチと画像化**:
  - **Android**: `NotificationListenerService` により通知をフック。タイトル、本文、受信時刻を 240×135 ピクセルのビットマップに描画し、16ビット Little-Endian RGB565 形式に変換。
  - **Linux (Ubuntu)**:
    - **Qt5 C++ GUI**: タスクトレイ常駐。D-Bus 通知（`org.freedesktop.Notifications`）をキャッチして RGB565 へ変換・送信。
    - **Python スクリプト (`ubuntu_notifier.py`)**: D-Bus 監視モードに加え、CLI からの任意画像（PNG/JPEG等）をリサイズ・送信可能。
- **データ転送プロトコル**:
  - **TCP (Wi-Fi)**: ポート `5555`。mDNS（`esp32-notify.local`）解決により固定 IP 不要。
  - **パケット構造**:
    - オプションヘッダー（4バイト）: `'N'`, `'T'`, `表示秒数 (uint16_t Big-Endian)`
    - 画像ペイロード: 64,800 バイト（240 × 135 × 2バイト, Little-Endian RGB565）
    - ※ヘッダーなしの場合はデフォルト10秒間表示。
- **表示および LED 制御**:
  - **ST7789 LCD**: 通知受信時にバックライト自動点灯。指定秒数経過後、通常ステータス画面に復帰。
  - **通知 LED (GPIO 7)**:
    - 通知画像表示中は `HIGH`（点灯）。
    - タイムアウト後または通常時は `LOW`（消灯）。
    - ESP-IDF の Light Sleep 時にも誤点灯・リークを防ぐため、`gpio_hold_en()` による状態保持を実装。

---

### 2. スマートホーム (Matter) 対応 & Linux 電源・音量制御

ESP32-C6 が Matter 標準の **Dimmable Plug-in Unit** として動作し、スマートホームエコシステム（Google Home, Apple Home など）から PC を家電のように統合管理できます。

- **Matter エンドポイント構成**:
  - **OnOff クラスタ (`0x0006`)**: PC の電源 ON / OFF 制御。
  - **LevelControl クラスタ (`0x0008`)**: レベル（0〜254）操作を PC の音量（0〜100%）にマッピング。
- **配線不要の電源制御 (Wiring-Free)**:
  - **リモート起動 (電源 ON)**:
    - Matter から ON コマンドを受信すると、対象 PC の MAC アドレス宛てに Wake-on-LAN (WOL) マジックパケット（UDP ポート 9）をブロードキャスト送信。
  - **リモートシャットダウン (電源 OFF)**:
    - Matter から OFF コマンドを受信すると、Linux PC 上の制御デーモン (`ubuntu_shutdown_daemon.py`, TCP ポート 7777) へ事前共有された安全な認証トークン（`TOKEN_SHUTDOWN`）を送信。
    - デーモン側で `sudo shutdown -h now` を安全に実行。
- **双方向 PC 状態監視 & Matter 属性自動同期**:
  - PC 本体の USB ポートに直結された ESP32-C6 が、2秒周期で USB Serial/JTAG（`usb_serial_jtag_is_connected()`）の接続状態をポーリング。
  - PC の実電源状態（`ON` / `OFF` / `BOOTING`）を正確に判定。
  - 手動起動や手動シャットダウン時も、Matter 上のステータスが自動同期（誤動作防止ガード `s_syncing_attribute` 付き）。
- **省電力と画面保護**:
  - PC `OFF` 状態が 5分継続すると、LCD バックライトを自動消灯。
  - PC `OFF` 時は描画タイマーや時計点滅を停止し、フリッカー防止および低消費電力化。
  - PC 起動検知で LCD バックライトが自動復帰点灯。

---

### 3. 非常停止ボタンによる PC スクリーンロック

デスク上や作業スペースに設置された物理的な非常停止スイッチを押すことで、即座に Linux PC の画面をロックします。

- **ハードウェア設計 (フェイルセーフ仕様)**:
  - **ピン**: `GPIO 8`
  - **スイッチ仕様**: B接点（NC: Normally Closed / 常時閉）。
  - **動作原理**: 通常時はスイッチ経由で GND に短絡（`LOW`）。非常停止ボタン押下時（または配線切断・断線時）にスイッチが開き、ESP32 内部プルアップにより `HIGH` を検出。
  - **デバウンス処理**: ソフトウェアタイマー / カウンタによるチャタリング除去。
- **緊急停止シーケンス**:
  1. GPIO 8 の `HIGH` レベル遷移（非常停止作動）を検知。
  2. ESP32 から Linux 制御デーモン（TCP ポート 7777）へ `PC_CONTROL_TOKEN_EMERGENCY_STOP` を送信。
  3. デーモン側で以下の処理を即座に実行:
     - `notify-send -u critical "EMERGENCY STOP"` による緊急通知
     - `loginctl lock-sessions` / `loginctl lock-session <ID>` による全アクティブセッションの即時画面ロック
- **緊急解除シーケンス**:
  1. 非常停止ボタンを回して復帰させ、GPIO 8 が `LOW` へ復帰。
  2. ESP32 から Linux 制御デーモンへ `PC_CONTROL_TOKEN_EMERGENCY_RELEASE` を送信。
  3. デーモン側で `loginctl unlock-sessions` を発行し、解除通知を表示。

---

## 🔌 ハードウェア接続 & GPIO ピンアサイン

ESP32 ボード: **Waveshare ESP32-C6-GEEK**

| ピン番号 | 信号名 | 入出力 | 機能説明 |
|:---:|---|:---:|---|
| **GPIO 1** | `LCD_PIN_SCLK` | 出力 | SPI クロック (ST7789 ディスプレイ) |
| **GPIO 2** | `LCD_PIN_MOSI` | 出力 | SPI データ (ST7789 ディスプレイ) |
| **GPIO 3** | `LCD_PIN_DC`   | 出力 | データ / コマンド選択 |
| **GPIO 4** | `LCD_PIN_RST`  | 出力 | LCD ハードウェアリセット |
| **GPIO 5** | `LCD_PIN_CS`   | 出力 | SPI チップセレクト |
| **GPIO 6** | `LCD_PIN_BL`   | 出力 | バックライト輝度 / ON-OFF 制御 |
| **GPIO 7** | **`GPIO_NOTIFY_LED`** | **出力** | **通知 LED（通知表示中 HIGH、通常時 LOW）** |
| **GPIO 8** | **`GPIO_EMERGENCY_STOP`** | **入力** | **非常停止ボタン（B接点 NC / 内部プルアップ / 押下で HIGH）** |
| **GPIO 9** | `GPIO_BOOT_BUTTON` | 入力 | ボード搭載 BOOT ボタン（短押し: LCD ON/OFF、長押し: Factory Reset） |
| **USB** | USB Type-A | 電源/通信 | PC直結給電 & USB CDC/JTAG による PC 電源状態判定 |

---

## 📡 ネットワーク & ポート仕様

| プロトコル | ポート | 役割 | 備考 |
|---|---|---|---|
| **TCP** | `5555` | 通知画像データ受信 | mDNS ホスト: `esp32-notify.local` |
| **TCP** | `7777` | Linux 制御デーモン受信 | トークン認証（シャットダウン / 非常停止） |
| **UDP** | `9` | Wake-on-LAN (WOL) | PC リモート起動用マジックパケット |
| **UDP** | `5540` | Matter 通信 | スマートホーム制御プロトコル |
| **mDNS** | `5353` | サービス検出 | `_matter._tcp`, `_notify._tcp` |

---

## 📁 ディレクトリ構成

```text
.
├── SYSTEM_OVERVIEW.md             # 本仕様書
├── README.JP.md                   # 通知システム全体のREADME
├── README.md                      # グローバルREADME (英語)
├── app/                           # Android 通知転送クライアントアプリ (Kotlin)
├── Linux/                         # Linux Qt5 C++ 常駐通知クライアント
└── ESP32-c6/                      # ESP32-C6-GEEK ファームウェア & PC用デーモン
    ├── main/                      # ESP-IDF ソースコード
    │   ├── app_main.cpp           # メイン初期化、Matter イベント、非常停止タスク
    │   ├── notify_server.c        # TCP 5555 通知画像受信サーバー & mDNS
    │   ├── lcd_display.cpp        # ST7789 LCD描画制御 & 通知LED (GPIO 7)
    │   ├── pc_monitor.c           # USB JTAGによるPC状態監視
    │   ├── pc_control.c           # WOL パケット送信
    │   ├── shutdown_client.c      # PC制御トークン送信クライアント (TCP 7777)
    │   └── wifi_creds.h           # Wi-Fi SSID, WOL MAC, 認証トークン設定
    ├── ubuntu_notifier.py         # Linux D-Bus 通知転送 & 任意画像送信スクリプト
    ├── ubuntu_shutdown_daemon.py  # Linux 常駐制御デーモン (シャットダウン/画面ロック)
    └── ubuntu_shutdown_daemon.sh  # デーモン起動用シェルスクリプト
```

---

## 🚀 導入・実行手順まとめ

### 1. ESP32-C6 ファームウェアのセットアップ
1. `ESP32-c6/main/wifi_creds.h` に Wi-Fi 情報・PC MAC アドレス・制御トークンを設定。
2. ESP-IDF v5.4 環境にてビルド & 書き込み:
   ```bash
   cd ESP32-c6
   idf.py set-target esp32c6
   idf.py build flash monitor
   ```
3. 起動後、Google Home または Apple Home アプリから Matter デバイスとしてペアリング。

### 2. Linux 側のセットアップ
1. **制御デーモンの常駐**:
   ```bash
   cd ESP32-c6
   ./ubuntu_shutdown_daemon.sh
   ```
   *(自動起動サービスとして systemd 等に登録推奨)*
2. **通知転送クライアントの実行**:
   - Qt5 GUI 版: `Linux/` で `cmake` ビルドして実行。
   - または Python 版: `python3 ESP32-c6/ubuntu_notifier.py`

### 3. Android 側のセットアップ
1. Android Studio で `app` プロジェクトを開き、端末にインストール。
2. アプリの「通知へのアクセス設定」を許可。
3. 送信先ホストに `esp32-notify.local`（または ESP32 の IP アドレス）を指定。
