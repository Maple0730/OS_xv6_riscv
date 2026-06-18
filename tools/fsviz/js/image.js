class ImageOption {
    constructor(option) {
        this.option = option;
        this.element = document.createElement("div");

        this.inputElement = document.createElement("input");
        this.inputElement.name = 'file';
        this.inputElement.value = option.path;
        this.inputElement.type = 'radio';
        this.inputElement.id = option.id;
        this.inputElement.onchange = () => main(option.path);
        this.element.appendChild(this.inputElement);


        const textElement = document.createElement("pre");
        textElement.textContent = option.label;

        const labelElement = document.createElement("label");
        labelElement.htmlFor = option.id;
        labelElement.appendChild(textElement);

        this.element.appendChild(labelElement);
    }

    check() {
        this.inputElement.checked = true;
        this.inputElement.onchange();
    }

    uncheck() {
        this.inputElement.checked = false;
    }
}

const imageObjects = Config.imageOptions.map(option => new ImageOption(option));

imageObjects.forEach(image => Elements.imageListContainer.appendChild(image.element));

Elements.fileUpload.onchange = (e) => {
    if (e.target.files.length === 0) return;
    imageObjects.forEach(e => e.uncheck());
    main(e.target.files[0]);
};

async function imagePathExists(path) {
    try {
        const response = await fetch(path, { method: "GET" });
        return response.ok;
    } catch (_) {
        return false;
    }
}

async function autoSelectImage() {
    for (let i = 0; i < Config.imageOptions.length; i++) {
        const option = Config.imageOptions[i];
        if (await imagePathExists(option.path)) {
            imageObjects[i].check();
            return;
        }
    }
}

autoSelectImage();
