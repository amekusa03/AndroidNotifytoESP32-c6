# SmartOn PC (Waveshare ESP32-C6-GEEK Edition)

[English](README.md) | [日本語](README.jp.md)

Waveshare **ESP32-C6-GEEK** を使用し、PC（Ubuntu 等）を **Matter** プロトコル対応スマートホームデバイスとして制御するファームウェアです。  
Google Home / Nest Hub / Apple Home 等の標準スマートホームエコシステムからの音声操作や自動化ルールによって PC のリモート起動（Wake-on-LAN）、リモートシャットダウン、および音量調整が可能です。

---

## ✨ 主な特徴

- **Windows / Ubuntu 両対応**: USB 接続検知、WOL、および TCP デーモンの組み合わせにより Windows/Ubuntu 両 OS で利用可能。
- **専用配線の完全廃止 (Wiring-Free)**: メカリレーやフォトカプラ等のマザーボード配線は一切不要。PC の USB ポートに挿すだけで導入可能。
- **Wake-on-LAN (WOL) リモート起動**: Matter から ON 操作を受信した際、設定された MAC アドレス宛てに Wake-on-LAN マジックパケットを送信して PC を起動。
- **TCP トークン制御による安全なシャットダウン & 音量制御**: Matter から OFF 操作や LevelControl（音量調整）を受信した際、PC 上で動作する常駐デーモンへ認証トークンを送信してシャットダウンや音量変更を安全に執行。
- **Matter Dimmable Plug-in Unit 対応**: OnOff に加え `LevelControl` クラスタ（0〜254）に対応し、Google Home 等のレベル調整操作を PC 音量パーセンテージ（0〜100%）として連携。
- **USB 通信状態ベースの PC 監視**: 2秒周期で USB Serial/JTAG 接続状態（`usb_serial_jtag_is_connected()`）を監視し、PC の実態（ON / OFF / BOOTING）を正確に判定。
- **Matter 属性自動同期 & フェイルセーフ**: PC の実態と Matter 属性値の乖離を検出した際、誤操作防止の安全ガード（`s_syncing_attribute`）付きで Matter 属性を自動更新・自動同期。
- **Ubuntu デスクトップ通知表示 (ESP32Notify 統合)**:
  - Ubuntu の D-Bus 通知をキャッチし、240×135 の通知カード（タイトル・本文・時刻）を ST7789 LCD にリアルタイム表示（TCP ポート `5555`）。
  - 通知受信時にバックライトを自動点灯し、10秒後に自動で通常ステータス画面に復帰。
- **ST7789 カラー LCD リアルタイム表示**: 1.14 インチ LCD に Wi-Fi 接続状態、割り当て IP アドレス、PC 状態、動作ステータス・音量変更ログをグラフィカルに表示。
- **省電力＆画面保護・フリッカー防止**:
  - PC が `OFF` になってから 5 分経過すると LCD バックライト（輝度）を自動で `0`（消灯）に移行。PC 起動・復帰で自動点灯。本体 BOOT ボタン（GPIO 9）短押しで手動 ON/OFF 切替も可能。
  - PC `OFF` 時は画面の毎秒再描画および時計コロンの点滅アニメーションを自動停止し、画面チラつき（フリッカー）を防止。
  - ESP-IDF `esp_pm` (Power Management) と連携し、周波数動的制御と自動ライトスリープで省電力を最適化。
- **Google Home 互換性最適化**:
  - Wi-Fi 省電力モードの動的制御による IPv6 mDNS（`_matter._tcp`）ドロップ防止。
  - MAC アドレスベースの決定論的 UniqueID（`chip-config/unique-id`）自動生成。

---

## 🛠️ ハードウェア仕様

- **マイコンボード**: Waveshare ESP32-C6-GEEK
- **ディスプレイ**: 1.14 インチ ST7789 カラー LCD (240 × 135)
- **接続方式**: USB Type-A ドングル（PC の USB ポートに直結）
- **物理配線**: なし（USB 給電および接続検知）
- **物理ボタン**: GPIO 9 (BOOT ボタン短押しで画面 ON/OFF / 長押し 3 秒以上で Factory Reset)
- **非常停止ボタン**: GPIO 8 (B接点 NC、押下で HIGH → PC へ緊急停止トークン送信)

### GPIO ピン割り当て

| GPIO | 信号名 | 方向 | 用途 |
|:---:|---|:---:|---|
| 1 | `LCD_PIN_SCLK` | OUT | SPI クロック (ST7789) |
| 2 | `LCD_PIN_MOSI` | OUT | SPI データ (ST7789) |
| 3 | `LCD_PIN_DC` | OUT | データ / コマンド選択 (ST7789) |
| 4 | `LCD_PIN_RST` | OUT | ハードウェアリセット (ST7789) |
| 5 | `LCD_PIN_CS` | OUT | チップセレクト (ST7789) |
| 6 | `LCD_PIN_BL` | OUT | バックライト制御（HIGH = 点灯） |
| **7** | **`GPIO_NOTIFY_LED`** | **OUT** | **通知 LED 出力（通知画像表示中 HIGH、通常時 LOW）** |
| **8** | **`GPIO_EMERGENCY_STOP`** | **IN** | **非常停止ボタン（B接点 / 内部プルアップ、GND接続、押下で HIGH）** |
| 9 | `GPIO_BOOT_BUTTON` | IN | BOOT ボタン（内部プルアップ、押下で LOW）|

---

## 💻 ソフトウェアスタック

| レイヤー | 名称 | バージョン / 設定 |
|---|---|---|
| OS / SDK | ESP-IDF | v5.4.1 |
| ターゲット | ESP32-C6 | `esp32c6` |
| スマートホーム規格 | esp-matter / Matter | Wi-Fi (`kOnNetwork`) |
| デバイスタイプ | Dimmable Plug-in Unit | Cluster: `0x0006` (OnOff), `0x0008` (LevelControl) |
| 電源制御方式 | Wake-on-LAN (WOL) / TCP Daemon | UDP Port 9 (WOL) / TCP Port 7777 (Control Daemon) |

---

## ⚙️ ネットワーク設定 (`main/wifi_creds.h`)

セキュリティ上の理由により、SSID やパスワード、MAC アドレス等の認証情報は Git 管理対象外（`.gitignore` に登録）となっています。  
初回ビルド前に `main/wifi_creds.h` を作成し、お使いの環境に合わせて以下の内容を設定してください。

### `main/wifi_creds.h` テンプレート

```c
#pragma once

// Wi-Fi 接続情報
#define WIFI_SSID     "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"

// ターゲット PC 設定
#define PC_HOSTNAME    "your-pc-hostname"     // mDNSホスト名（.localなし）
#define PC_MAC_ADDRESS "XX:XX:XX:XX:XX:XX"    // WOL対象PCのMACアドレス

//#define PC_STATIC_IP "192.168.11.13"        // オプション: mDNSをスキップして直接IP指定する場合

// PC 制御用ポート & トークン設定
#define PC_CONTROL_PORT              7777
#define PC_CONTROL_TOKEN_SHUTDOWN    "secure_pc_shutdown_token_12345"
#define PC_CONTROL_TOKEN_EMERGENCY_STOP    "secure_pc_emergency_stop_token_12345"
#define PC_CONTROL_TOKEN_EMERGENCY_RELEASE "secure_pc_emergency_release_token_12345"
```

---

## 🚀 ビルド・書き込み手順

### 前提条件

- ESP-IDF v5.4.1 および `esp-matter` 環境がセットアップ済みであること
- `main/wifi_creds.h` が作成されていること

### ビルド & 書き込み

```bash
# 1. 環境変数のロード
source $HOME/esp/esp-idf/export.sh
source $HOME/esp/esp-matter/export.sh

# 2. ターゲット設定
idf.py set-target esp32c6

# 3. ビルド
idf.py build

# 4. 書き込み & モニター起動
idf.py -p /dev/ttyACM0 flash monitor
```

### 全消去（Erase）して再ペアリングする場合

```bash
# NVS / Matter ファブリック情報の完全消去
idf.py -p /dev/ttyACM0 erase-flash

# 再書き込み
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 📱 Google Home ペアリング情報

- **方式**: On-Network Commissioning (Wi-Fi)
- **手動ペアリングコード**: `34970112332`
- **QR コード表示 URL**: [https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00](https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00)
- **ファクトリーリセット**: 本体 GPIO 9 ボタンを 3 秒以上長押し

---

## 🔄 動作フロー

1. **電源 ON 操作 (Matter -> PC)**
   - Google Home 等から ON 操作を受信。
   - ESP32-C6 から Wi-Fi 経由で Wake-on-LAN (WOL) マジックパケット（`PC_MAC_ADDRESS` 宛）を送信。
   - PC が起動。

2. **電源 OFF 操作 (Matter -> PC)**
   - Google Home 等から OFF 操作を受信。
   - ESP32-C6 から Wi-Fi 経由で PC の `PC_CONTROL_PORT` 宛てに TCP 接続。
   - `PC_CONTROL_TOKEN_SHUTDOWN`（シャットダウントークン）を送信。
   - PC 側の常駐プログラム（デーモン）がトークンを認証し、`sudo shutdown -h now` を執行。

3. **PC 状態監視 & LCD 制御 (USB Status -> Matter / LCD)**
   - 2秒周期で USB Serial/JTAG 接続状態を監視。
   - 接続あり ➔ `PC: ON`（LCD 表示更新）
   - 接続なし ➔ `PC: OFF`（LCD 表示更新）
   - 状態乖離を検出した際、自動で Matter の OnOff 属性を同期更新。
   - **`PC: OFF` が 5 分間継続すると LCD バックライトを自動消灯（輝度0）。PC 起動で自動復帰。**
   - **`PC: OFF` 時は時計コロンの点滅および画面描画処理をサスペンドし、画面フリッカーを抑止。**

---

4. **非常停止ボタン入力 & 解除 (GPIO 8 → PC)**
   - **配線**: `GPIO 8` ─── [B接点 (NC) 非常停止SW] ─── `GND`
   - **通常時 (非押下)**: GND と導通しているため、GPIO 8 = LOW。
   - **押下時 (停止作動)**: スイッチが開放され内部プルアップにより GPIO 8 = HIGH（立ち上がりエッジ検出）。
     - ESP32-C6 が `PC_CONTROL_TOKEN_EMERGENCY_STOP` を PC 宛 TCP で非同期送信。
     - LCD に `EMRG STOP!` を表示。
   - **解除時 (復帰作動)**: ボタン解除で GND と再導通し GPIO 8 = LOW（立ち下がりエッジ検出）。
     - ESP32-C6 が `PC_CONTROL_TOKEN_EMERGENCY_RELEASE` を PC 宛 TCP で非同期送信。
     - LCD に `EMRG Released` を表示。
   - 100ms の安定検出（デバウンス処理）により、スイッチのチャタリング誤動作を抑止。
   - ※ フェールセーフ設計: ケーブル抜けや断線時も回路が開放されて HIGH となり、安全に非常停止が作動します。

---

## 🐧 Ubuntu 側の常駐プログラム（デーモン）設定

ESP32 からの要求（TCP トークン）を待ち受け、PC のシャットダウンや非常停止イベント（停止・解除）の受信を行うための常駐デーモンを systemd を使用して設定します。

### 1. デーモンスクリプトの確認・パーミッション設定

リポジトリ直下の `ubuntu_shutdown_daemon.py` を使用します。
スクリプト内の `PC_CONTROL_PORT` と各 `TOKEN_*` が `wifi_creds.h` の設定と一致しているか確認してください。

> [!SECURITY]
> デーモンスクリプトには認証トークン（秘密情報）が直接書き込まれているため、root 以外のユーザーが読み書きできないように権限を制限することを推奨します。
> ```bash
> # 所有者を root に変更し、root のみ読み書き・実行可能に設定
> sudo chown root:root ubuntu_shutdown_daemon.py
> sudo chmod 700 ubuntu_shutdown_daemon.py
> ```

### 2. systemd サービスファイルの作成
`/etc/systemd/system/pc-shutdown-daemon.service` を作成し、デーモンをシステムサービスとして登録します。

まず、スクリプトをシステムの実行ファイル標準ディレクトリ `/usr/local/bin/` にコピーします。

```bash
# スクリプトを標準ディレクトリにコピー
sudo cp ubuntu_shutdown_daemon.py /usr/local/bin/
sudo chown root:root /usr/local/bin/ubuntu_shutdown_daemon.py
sudo chmod 700 /usr/local/bin/ubuntu_shutdown_daemon.py
```

次に、サービスファイルを作成します：

```bash
sudo nano /etc/systemd/system/pc-shutdown-daemon.service
```

以下の内容を貼り付けます：

```ini
[Unit]
Description=ESP32 SmartOn PC Shutdown & Control Daemon
After=network.target

[Service]
Type=simple
# /usr/local/bin/ に配置したスクリプトを指定します。
ExecStart=/usr/bin/python3 /usr/local/bin/ubuntu_shutdown_daemon.py
Restart=always
RestartSec=5
User=root

[Install]
WantedBy=multi-user.target
```

### 3. サービスの有効化と起動

作成したサービスをリロードし、自動起動の設定および手動起動を行います。

```bash
# 1. systemdの再読み込み
sudo systemctl daemon-reload

# 2. 自動起動（OS起動時に自動で立ち上がるよう設定）
sudo systemctl enable pc-shutdown-daemon.service

# 3. デーモンの起動
sudo systemctl start pc-shutdown-daemon.service
```

### 4. 動作・ログ確認方法

正常に起動しているか、ログが出ているか確認します。

```bash
# 状態確認（Active: active (running) になっていれば正常）
sudo systemctl status pc-shutdown-daemon.service

# リアルタイムログ監視（ESP32から接続された際のログなどを確認できます）
sudo journalctl -u pc-shutdown-daemon.service -f
```

---

---

## 📢 LCD 通知表示プロトコル仕様 (TCP Port 5555)

ESP32-C6-GEEK の 1.14 インチ LCD (240×135) に、PC や外部プログラムからネットワーク（TCP）経由で任意の通知画像を表示させることができます。  
独自の常駐アプリ（Python, C#, Node.js, Go, Rust 等）から通知を送信する際は、以下のプロトコル仕様に従ってパケットを送信してください。

### 1. 通信仕様

| 項目 | 設定値 / 仕様 |
|---|---|
| **プロトコル** | TCP (Socket) |
| **デフォルトポート** | `5555` (`NOTIFY_TCP_PORT`) |
| **画面解像度** | **横 240 × 縦 135 ピクセル** (アスペクト比 16:9) |
| **カラー形式** | **RGB565** (1ピクセル = 16bit = 2バイト, R:5bit / G:6bit / B:5bit) |
| **バイトオーダー** | リトルエンディアン (Little-Endian: `[下位バイト, 上位バイト]`) |
| **総データサイズ** | **64,804 バイト** (NTヘッダー 4バイト + 画像データ 64,800バイト) <br> *(ヘッダーなし生画像 64,800 バイトも互換受付可能)* |

---

### 2. パケット構造

#### ① NTヘッダー付きパケット (推奨 / 計 64,804 バイト)

通知の表示継続時間（秒）を任意に指定できます。

```
+---------------+---------------+------------------+------------------+--------------------------------------+
| Byte 0        | Byte 1        | Byte 2           | Byte 3           | Byte 4 ～ 64803 (計 64,800 バイト)   |
| 'N' (0x4E)    | 'T' (0x54)    | Duration Hi (MSB)| Duration Lo (LSB)| 240x135 RGB565 ピクセルデータ         |
+---------------+---------------+------------------+------------------+--------------------------------------+
```

- **マジックバイト**: `'N'` (`0x4E`), `'T'` (`0x54`)
- **表示時間 (Duration)**: 16-bit Big-Endian 整数（秒単位、`1` 〜 `600` 秒）。
  - 例: 10秒表示 ➔ `0x00, 0x0A`
  - 例: 30秒表示 ➔ `0x00, 0x1E`
  - ※ 0 または範囲外が指定された場合はデフォルト値（10秒）が適用されます。
- **ピクセルデータ**: 240 × 135 = 32,400 ピクセル × 2 バイト = **64,800 バイト**。左上 `(0,0)` から右下 `(239,134)` への走査線順（Row-major order）。

#### ② ヘッダーなし生データ (互換モード / 計 64,800 バイト)

ヘッダーを付与せず 64,800 バイトの RGB565 生データを直接送信した場合、デフォルト時間（10秒）で表示されます。

---

### 3. RGB565 ピクセル変換式

各ピクセルの RGB 値（各 0〜255）から 16bit 値を計算し、リトルエンディアン（下位バイト先頭）で書き込みます。

```python
# RGB888 (r, g, b: 0~255) から RGB565 16bit への変換
rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

# Little-Endian として 2 バイト格納
lo_byte = rgb565 & 0xFF
hi_byte = (rgb565 >> 8) & 0xFF
```

---

### 4. 送信プログラム実装例 (Python)

Pillow (PIL) を使って 240×135 の通知カードを描画し、ESP32 へ TCP 送信する最小サンプルです。

```python
import socket
import struct
from PIL import Image, ImageDraw, ImageFont

ESP32_IP = "192.168.11.100"  # ESP32のIPアドレス
PORT = 5555
WIDTH, HEIGHT = 240, 135
DURATION_SEC = 10  # 表示秒数 (秒)

# 1. 240x135 の通知画像を生成 (ダークテーマ例)
img = Image.new("RGB", (WIDTH, HEIGHT), color=(15, 23, 42))
draw = ImageDraw.Draw(img)

# タイトルバー & 本文の描画
draw.rectangle([(0, 0), (WIDTH, 24)], fill=(30, 41, 59))
draw.text((10, 5), "System Alert", fill=(56, 189, 248))
draw.rounded_rectangle([(6, 30), (WIDTH - 6, HEIGHT - 6)], radius=4, fill=(24, 34, 53), outline=(51, 65, 85))
draw.text((16, 40), "Meeting in 5 mins!", fill=(255, 255, 255))
draw.text((16, 65), "Zoom: Weekly Standup", fill=(203, 213, 225))

# 2. RGB565 (Little-Endian) バイト列へ変換
pixels = img.load()
pixel_bytes = bytearray()
for y in range(HEIGHT):
    for x in range(WIDTH):
        r, g, b = pixels[x, y]
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixel_bytes.extend(struct.pack("<H", rgb565))  # '<H': 16bit Little-Endian

# 3. NTヘッダー (4バイト) + ピクセルデータ (64800バイト) を組み立て
header = struct.pack(">2sH", b"NT", DURATION_SEC)  # '>2sH': 'NT' + 16bit Big-Endian
packet = header + bytes(pixel_bytes)

# 4. TCPソケットで送信
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.settimeout(5.0)
    s.connect((ESP32_IP, PORT))
    s.sendall(packet)

print("✓ 通知を送信しました!")
```

---

### 5. 画面の挙動・消灯連動

- **自動点灯**: LCD バックライトが消灯（スリープ）している状態で通知パケットを受信すると、自動的にバックライトが点灯して通知を表示します。
- **自動復帰**: 指定した秒数（デフォルト 10 秒）が経過すると、自動的にメインのステータス画面（時計・Wi-Fi/PC状態）に戻ります（元が消灯状態だった場合は再び消灯します）。
- **手動閉じる**: 本体側面の BOOT ボタン（GPIO 9）を短押しすることで、表示秒数の経過を待たずに即時通知を閉じてメイン画面に戻すことができます。
- **通知 LED 連動 (GPIO 7)**: 通知画像を表示している間は GPIO 7 が HIGH に設定されます。通常のステータス画面に戻った時点で LOW に復帰します。ライトスリープ中も `gpio_hold` により状態が保持されます。

---

## 📄 ライセンス

本プロジェクトは MIT ライセンスの下で公開されています。詳細は [LICENSE](LICENSE) を参照してください。
