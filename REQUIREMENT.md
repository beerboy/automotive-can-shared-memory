# 目的
車両のCANデータのようなものを共有メモリに格納し、さまざまなアプリがCANデータをGet, Subscribeするような機能を構築する

# 要求仕様
提供する関数として、Set, Get, Subscribeの３種類用意する。
CANデータはPDU形式として、CAN ID:4[byte](29bit有効値), データ長(DLC):2[byte], データ部:0~64[byte]で構成されるデータである。
CANデータは非同期に任意のタイミングで受信を行う。
Get, Subscribeを行う関数は複数(n個)あり、任意のタイミングで呼び出される。

C/C++で実装を行うこと。
適宜必要なStub, Mockも用意すること。
必要な補助関数なども別途使用定義し、実装を行うこと。
ビルドシステムにはCMakeを使用すること。
最終的にはLinux環境での動作を想定しており、開発にはPodman + UBI等のLinuxコンテナ環境を使用すること。

## Set関数
入力: CANデータ
SetはCANデータを共有メモリに格納する。
Subscribeとしてデータ更新があった際に、通知する機能が必要。

## Get関数
入力: CAN ID
CAN IDを元に現在格納されている共有メモリからCANデータを出力する

## Subscribe関数
入力: CAN ID,  購読回数、タイムアウト時間
CAN IDを元に更新の度にCANデータを返す。
購読回数分のデータを返すと関数は正常終了として終了する。
購読回数に0が設定された場合、無限回と判断する。
タイムアウト時間が過ぎても更新がない場合、タイムアウトが発生した旨を通知し関数は終了する。
タイムアウト時間<0に設定されている場合、タイムアウト処理は行わない。