const Config = {
    entrySize: 16,
    inodeSize: 64,
    blockSize: 1024,
    numberOfDirectAddress: 12,
    rootInode: 1,
    fsMagic: 0x10203040,
    superBlockIndex: 1,
    bitsPerBitmapBlock: 1024 * 8,

    imageOptions: [
        {
            id: "current-build",
            label: "Current build/fs.img",
            path: "../../build/fs.img",
        },
        {
            id: "local-copy",
            label: "Local tools/fsviz/fs.img",
            path: "fs.img",
        },
    ],
};
