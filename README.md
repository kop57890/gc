# gc
1. 必須為Ubuntu 16.04
2. 把library加入bashrc內
    vi /home/YOUR_FOLDER/.bashrc
    接著到最後一行加入
    export LD_LIBRARY_PATH=/home/YOUR_FOLDER/gc/ifly/libs/x64
    :wq 之後
    source .bashrc
3. cd /home/YOUR_FOLDER/gc/ifly/samples/aiui_sample/build
4. ./demo 便可開始執行

給江大哥的版本是在ifly資料夾內
目前江大哥的版本是我把cin這個功能拿掉的版本
我在自己這邊使用如果不加上cin, 在得到回傳答案之前process就會結束掉

如果江大哥想自己compiler的話 執行aiui_sample裡面的run.sh
sh run.sh
執行完後新的執行檔就會在build資聊夾內的demo
