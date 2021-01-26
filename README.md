# Upload

```bash
platformio run --target upload
```

## Single strain

Individual strain consists of two 1k resistors connected as a voltage
divider. Connection with two fixed 1k resistors as a voltage divider
creates a full bridge. **50kg max.**

![Single strain half bridge with two additional 1k resistors
breadboard view](./docs/1-strain_bb.jpg)

![Single strain half bridge with two additional 1k resistors
schematic](./docs/1-strain_schem.jpg)

## Double strain

Using two single strain half bridges together forms a full bridge. **100kg
max.**

![Two single strain half bridges togerher breadboard
view](./docs/2-strain_bb.jpg)

![Two single strain half bridges togerher breadboard
schematic](./docs/2-strain_schem.jpg)

## Quadruple strain

Using four single strain half bridges together again forms a full bridge,
with the resistance doubled. **200kg max.**

![Two single strain half bridges togerher breadboard
view](./docs/4-strain_bb.jpg)

![Two single strain half bridges togerher breadboard
schematic](./docs/4-strain_schem.jpg)
