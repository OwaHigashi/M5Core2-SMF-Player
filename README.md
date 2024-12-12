# M5Stack Simple SMF Player

# 2048_M5Stack
## M5Core2ライブラリプロジェクト
M5Core2のSD-Updaterを用いたライブラリ集を構築するプロジェクトの一環として実装されています。

## どんなアプリ？

こちらは、UNIT-SYNTHに対応した非常にシンプルなSMFプレーヤで、オリジナルは特に画面表示は何も行いませんでしたが、
@catsin さんのM5Stack + MIDIインタフェース用アプリ ( https://bitbucket.org/kyoto-densouan/smfseq/src/m5stack/ )さらにはこれを基にしたnecobitさんによる画面描画部分を参考に全て最初から再実装しています。

#include "MD_MIDIFile.h"

とあるように、MD_MIDIFileライブラリを利用します。このライブラリは、smfファイルを解析し、シリアルポートにMIDI信号を送信するために必要な全ての前処理を行うことができます。

再生したいファイルを、microSDの/smfフォルダから探し出してプレイリストを作成、これを再生します。

- M5Stack Basic (CORE) を対象としていますが、Core2も含めて、出力先ポートを修正することで容易に対応可能です
- 音源は、UNIT-SYNTHを利用します

システムエクスクルーシブメッセージにも対応しています。

## 操作方法

左ボタンで前の曲、右ボタンで次の曲、真ん中のボタンで再生と停止です。

## 再生における制限事項

- MIDI All Notes Off信号を送るタイミングが、再生開始時になるため、突然落ちる、アプリを終了させるなどすると、音が鳴りっぱなしになります。この場合は、一度電源を落としてください。これは、プログラム上ではなく、システム構成上の制約になります。

- 実装をシンプルにするために、smf format 0 のみ対応です。smf format 1および2のファイルは、format 0に変換する必要があります。変換するには、次のアプリが便利です。
  - GNMIDI: 高機能でエラーのあるファイルもある程度修正できます。有料です。次に紹介するアプリでエラーとなるファイルでも変換できます。なお、機能制限版は無償で使うことができ、機能制限版でも操作が煩雑になりますが、変換可能です。
  - SMFConverter 1.0b for Windows: 無料で利用でき、最低限の機能が備わっています。

## M5Stack-SD-Updater対応です

https://github.com/tobozo/M5Stack-SD-Updater 
https://github.com/lovyan03/M5Stack_LovyanLauncher 

# プログラマ

尾和 東/ぽこちゃ技術枠
