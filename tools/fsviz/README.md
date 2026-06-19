# xv6 fs.img Visualizer

这个目录保留了 `xv6-file-system-visualizer` 的界面和交互风格，但底层解析逻辑已经改成适配当前仓库这版 xv6 文件系统：

- `BSIZE = 1024`
- superblock 首字段包含 `magic`
- 磁盘布局包含独立的 `log` 区
- inode 区和 bitmap 区起始位置从 superblock 读取

## 使用方法

## 方式一：从仓库根目录启动

在仓库根目录启动一个静态文件服务：

```bash
python3 -m http.server 8000
```

然后打开：

```text
http://127.0.0.1:8000/tools/fsviz/
```

这时页面会优先自动加载：

```text
../../build/fs.img
```

也就是当前仓库生成出来的：

```text
build/fs.img
```

## 方式二：从 `tools/fsviz/` 目录启动

如果你就是想在 `tools/fsviz/` 目录里直接开 server，需要先同步一份本地镜像：

```bash
bash sync-fsimg.sh
python3 -m http.server 8000
```

然后打开：

```text
http://127.0.0.1:8000/
```

这时页面会先尝试：

```text
../../build/fs.img
```

如果因为 server 根目录限制无法访问，再自动回退到：

```text
fs.img
```

## 说明

如果你直接在 `tools/fsviz/` 里启动 `http.server`，浏览器是不能越过 server 根目录访问上级 `../../build/fs.img` 的，所以会出现：

```text
GET /build/fs.img 404
```

这是静态文件服务根目录限制，不是解析器本身出错。

如果你想看别的镜像，也可以直接用页面上的上传按钮手动选择。

## 颜色说明

- `K`：Boot Block
- `S`：Super Block
- `L`：Log Block
- `I`：Inode Block
- `B`：Bitmap Block
- `D`：Data Block
- `-`：Unused Block
