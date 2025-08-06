# BLE-Data-Exfiltration

## Using the Release

Set your "server" (any Windows System, BLE had issues for me with connecting to the service when running within a VM) to developer mode

After installing the UWP App:
1) Set your Windows into "Developer Mode"

![Developer Mode](image-1.png)

2) Allow the UWP App to store local files (Details see chapter settings: https://www.jasongaylord.com/blog/2021/11/17/uwp-file-access-denied).

![alt text](image.png)

3) On the client: Navigate to  https://rafaelscheel.github.io/BLE-Data-Exfiltration/client.html (the code is in the docs folder) in Chrome or Edge. Web Bluetooth can be disabled by policy: https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/defaultwebbluetoothguardsetting 


## Run Server from Visual Studio

It is recomended to run the Windows Universal Platform App from within VS. 
![alt text](image-2.png)

1) Build -> Rebuild BluetoothLE (not the Solution!)
2) Build -> Deoploy BluetoothLE (this installs the App on your computer)
3) Start with or without debugging. Without debugging will help the file transfer performance.