# antd-wterm-plugin
**wterm** is an [antd plugin](https://github.com/lxsang/ant-http) providing the Terminal gateway to the web using websocket.

## Build from source
As **wterm** is an **Antd's** plugin, it need to be built along with the server. This require the following application/libraries to be pre installed:

### build dep
* git
* make
* build-essential

### server dependencies
* libssl-dev
* libsqlite3-dev

### build
When all dependencies are installed, the build can be done with a few single command lines:

```bash
mkdir antd
cd antd
wget -O - https://apps.lxsang.me/script/antd | bash -s "wterm"
```
The script will ask you where you want to put the binaries (should be an absolute path, otherwise the build will fail) and the default HTTP port for the server config.

## Run
To run the Antd server with the **wterm** plugin:
```sh
/path/to/your/build/antd
```

Web applications can be put on **/path/to/your/build/htdocs**, the web socket to **wterm** is available at:
```
ws://your_host:your_port/wterm
```
This websocket address can be used with [xterm.js](https://xtermjs.org) to provide web based termnial application
