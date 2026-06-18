let superBlock;
let blockList;
let bitmapBlocks;

class BlockUtils {
    static init() {
        superBlock = new SuperBlock(Config.superBlockIndex);
        blockList = new Array(superBlock.size);
        bitmapBlocks = [];

        blockList[0] = new BootBlock(0);
        blockList[Config.superBlockIndex] = superBlock;

        for (let i = superBlock.logstart; i < superBlock.logstart + superBlock.nlog; i++)
            blockList[i] = new LogBlock(i);

        for (let i = superBlock.inodestart; i < superBlock.bmapstart; i++)
            blockList[i] = new InodeBlock(i);

        for (let i = superBlock.bmapstart; i < superBlock.dataStart; i++) {
            const bitmapBlock = new BitmapBlock(i, i - superBlock.bmapstart);
            bitmapBlocks.push(bitmapBlock);
            blockList[i] = bitmapBlock;
        }

        for (let i = superBlock.dataStart; i < superBlock.size; i++)
            blockList[i] = BlockUtils.isBlockAllocated(i) ? new DataBlock(i) : new UnusedBlock(i);

        for (let i = 0; i < blockList.length; i++) {
            if (!blockList[i])
                blockList[i] = new UnknownBlock(i);
        }
    }

    static render() {
        Elements.blockContainer.innerHTML = "";
        blockList.forEach(e => Elements.blockContainer.appendChild(e.getGridElement()));
        superBlock.gridElement.onmouseover();
    }

    static bitmapBlockFor(blockNumber) {
        const index = Math.floor(blockNumber / Config.bitsPerBitmapBlock);
        return bitmapBlocks[index];
    }

    static isBlockAllocated(blockNumber) {
        const bitmapBlock = BlockUtils.bitmapBlockFor(blockNumber);
        if (!bitmapBlock)
            return false;
        const localBit = blockNumber % Config.bitsPerBitmapBlock;
        return (bitmapBlock.dataView.getUint8(Math.floor(localBit / 8)) & (1 << (localBit % 8))) !== 0;
    }
}


class Block extends GridItem {
    constructor(blockNumber) {
        super();
        this.blockNumber = blockNumber;

        this.dataView = new DataView(image, Config.blockSize * blockNumber, Config.blockSize);
        this.uint8Array = new Uint8Array(this.dataView.buffer, this.dataView.byteOffset, this.dataView.byteLength);
        this.uint32Array = new Uint32Array(
            this.dataView.buffer,
            this.dataView.byteOffset,
            this.dataView.byteLength / Uint32Array.BYTES_PER_ELEMENT
        );
    }

    getDetailElement() {
        if (this.detailElement) return this.detailElement;

        this.detailElement = document.createElement("div");

        this.detailElement.appendChild(this.getErrorElement());
        this.detailElement.appendChild(this.getSummaryElement());
        this.detailElement.appendChild(this.getDataElement());

        return this.detailElement;
    }

    getSummaryElement() {
        const title = document.createElement("h4");
        title.innerText = "Contents in hexadecimal: ";
        return title;
    }

    getDataElement() {
        if (this.dataElement) return this.dataElement;
        this.dataElement = this.getHexDataElement();
        return this.dataElement;
    }

    getHexDataElement() {
        const element = document.createElement("pre");
        element.innerText = Array.from(this.uint32Array)
            .map(e => e.toString(16).padStart(8, '0'))
            .join(", \t");
        return element
    }

    getClassName() {
        return this.type.toLowerCase().replace(' ', '-');
    }

    getTitle() {
        return `Block ${this.blockNumber}: ${this.type}`;
    }

    isBlockAscii() {
        return false;
    }
}


class SuperBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);

        this.magic = this.dataView.getUint32(0, true);
        this.size = this.dataView.getUint32(4, true);
        this.nblocks = this.dataView.getUint32(8, true);
        this.ninodes = this.dataView.getUint32(12, true);
        this.nlog = this.dataView.getUint32(16, true);
        this.logstart = this.dataView.getUint32(20, true);
        this.inodestart = this.dataView.getUint32(24, true);
        this.bmapstart = this.dataView.getUint32(28, true);
        this.ninodeblocks = this.bmapstart - this.inodestart;
        this.dataStart = this.size - this.nblocks;
        this.nbitmapblocks = this.dataStart - this.bmapstart;

        this.type = "Super Block";
    }

    checkError() {
        if (this.magic !== Config.fsMagic)
            return `Bad FSMAGIC: expected 0x${Config.fsMagic.toString(16)}, got 0x${this.magic.toString(16)}.`;
        if (this.logstart !== 2)
            return `Unexpected log start block ${this.logstart}.`;
        if (this.inodestart >= this.bmapstart || this.bmapstart >= this.dataStart)
            return "Superblock layout fields are inconsistent.";
    }

    getSummaryElement() {
        const node = document.createElement("div");

        const title = document.createElement("h4");
        title.innerText = "Metadata: ";
        node.appendChild(title);

        const magic = document.createElement("p");
        magic.innerText = "Magic: 0x" + this.magic.toString(16);
        node.appendChild(magic);

        const size = document.createElement("p");
        size.innerText = "Image size (blocks): " + this.size;
        node.appendChild(size);

        const nblocks = document.createElement("p");
        nblocks.innerText = "Number of data blocks: " + this.nblocks;
        node.appendChild(nblocks);

        const ninodes = document.createElement("p");
        ninodes.innerText = "Number of inodes: " + this.ninodes;
        node.appendChild(ninodes);

        const nlog = document.createElement("p");
        nlog.innerText = "Number of log blocks: " + this.nlog;
        node.appendChild(nlog);

        const logstart = document.createElement("p");
        logstart.innerText = "Log start: " + this.logstart;
        node.appendChild(logstart);

        const inodestart = document.createElement("p");
        inodestart.innerText = "Inode start: " + this.inodestart;
        node.appendChild(inodestart);

        const bmapstart = document.createElement("p");
        bmapstart.innerText = "Bitmap start: " + this.bmapstart;
        node.appendChild(bmapstart);

        const datastart = document.createElement("p");
        datastart.innerText = "Data start: " + this.dataStart;
        node.appendChild(datastart);

        node.appendChild(super.getSummaryElement());

        return node;
    }

    getGridText() {
        return 'S';
    }
}

class BitmapBlock extends Block {
    constructor(blockNumber, bitmapIndex) {
        super(blockNumber);
        this.bitmapIndex = bitmapIndex;
        this.type = "Bitmap Block";
    }

    getSummaryElement() {
        const node = document.createElement("div");

        const title = document.createElement("h4");
        title.innerText = "Bitmap information: ";
        node.appendChild(title);

        const range = document.createElement("p");
        const start = this.bitmapIndex * Config.bitsPerBitmapBlock;
        const end = start + Config.bitsPerBitmapBlock - 1;
        range.innerText = `Covers block numbers ${start} .. ${end}`;
        node.appendChild(range);

        const content = document.createElement("h4");
        content.innerText = "Contents in binary: ";
        node.appendChild(content);

        return node;
    }

    getDataElement() {
        if (this.dataElement) return this.dataElement;

        this.dataElement = document.createElement("pre");
        this.dataElement.innerHTML = Array.from(this.uint8Array)
            .map(e => e.toString(2).padStart(8, '0'))
            .join(", \t");

        return this.dataElement;
    }

    getGridText() {
        return 'B';
    }
}

class DataBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);

        this.belongsToTextFile = false;
        this.isDirectoryBlock = false;

        this.type = "Data Block";
        this.gridText = 'D';
    }


    isBlockAscii() {
        return this.uint8Array.every(e => e < 128);
    }

    getSummaryElement() {
        const node = document.createElement("div");

        if (this.inode) {
            const title = document.createElement("h4");
            title.innerText = `Basic information: `;
            node.appendChild(title);


            const inode = document.createElement("p");
            inode.innerText = `Used by: inode ${this.inode.inum}`;
            node.appendChild(inode);

            const type = document.createElement("p");
            type.innerText = `Type: ${this.inode.typeName}`;
            node.appendChild(type);

            if (this.inode.pathList.length !== 0) {
                const path = document.createElement("p");
                path.innerText = `Path: ${this.inode.pathList.join(", ")}`;
                node.appendChild(path);

            }
        }


        if (this.isDirectoryBlock || this.belongsToTextFile) {
            const content = document.createElement("h4");
            content.innerText = "Contents: ";
            node.appendChild(content);
        } else {
            node.appendChild(super.getSummaryElement());
        }

        return node;
    }

    getDataElement() {
        if (this.dataElement)
            return this.dataElement;

        if (this.isDirectoryBlock) {
            this.dataElement = document.createElement("pre");
            const entries = this.getEntries();
            this.dataElement.innerHTML = Object.entries(entries).map(([name, inum]) => `${name} → ${inum}`).join("\n");
            return this.dataElement;
        }

        if (this.belongsToTextFile) {
            this.dataElement = document.createElement("pre");
            this.dataElement.innerText = new TextDecoder("utf-8").decode(this.dataView).replace(/\0/g, '');
            this.dataElement.classList.add("text");
            return this.dataElement;
        }

        this.dataElement = this.getHexDataElement();

        return this.dataElement;
    }


    getEntries() {
        if (this.entries) return this.entries;

        this.entries = {};
        for (let i = 0; i < Config.blockSize / Config.entrySize; i++) {
            const inum = this.dataView.getUint16(Config.entrySize * i, true);
            if (inum === 0) continue;

            const nameOffset = this.dataView.byteOffset + Config.entrySize * i + 2;
            const nameArray = new Uint8Array(this.dataView.buffer, nameOffset, Config.entrySize - 2);
            const name = new TextDecoder("utf-8").decode(nameArray).replace(/\0/g, '');

            this.entries[name] = inum;
        }

        return this.entries;
    }


    getRelatedDOMList() {
        return this.inode ? [this.inode.gridElement, ...this.inode.getRelatedDOMList()] : [];
    }

    checkError() {
        if (this.blockNumber < superBlock.dataStart)
            return false;

        if (!this.inode && !(this instanceof UnusedBlock)) {
            return "Bitmap marks block in use but it is not in use."
        }

        if (this.inode && this instanceof UnusedBlock) {
            return "Block used by inode but marked free in bitmap."
        }
    }

    getGridText() {
        return 'D';
    }
}


class InodeBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);
        this.type = "Inode Block";
    }

    getRelatedDOMList() {
        const numberOfInodesPerBlock = Config.blockSize / Config.inodeSize;
        return [...Array(numberOfInodesPerBlock).keys()]
            .map(i => i + numberOfInodesPerBlock * (this.blockNumber - superBlock.inodestart))
            .filter(i => i < inodeList.length)
            .map(i => inodeList[i].gridElement);
    }

    getGridText() {
        return 'I';
    }
}

class UnusedBlock extends DataBlock {
    constructor(blockNumber) {
        super(blockNumber);
        this.type = "Unused Block";
    }

    getGridText() {
        return '-';
    }
}

class BootBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);
        this.type = "Boot Block";
    }

    getGridText() {
        return "K";
    }
}

class LogBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);
        this.type = "Log Block";
    }

    getGridText() {
        return "L";
    }
}

class UnknownBlock extends Block {
    constructor(blockNumber) {
        super(blockNumber);
        this.type = "Unknown Block";
    }

    getGridText() {
        return "?";
    }
}
