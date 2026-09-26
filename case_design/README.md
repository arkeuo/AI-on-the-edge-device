# Enclosure for the Waveshare ESP32-S3-POE-ETH-CAM-KIT

Printed enclosure for a water meter reader built on this board.

| File | Part |
|---|---|
| `Lector.stl` | Body: a bell that sits on the meter, with a column that holds the board, RJ45/USB-C end up |
| `Tapa_Caixa.stl` | Sliding lid for the column |

- Lens tip to meter glass: about 50 mm.
- The camera flat cable is 76 mm long, and the straight run from the board connector to the camera module is 59 mm.
- The flash is an external WS2812B on GPIO17, set to `flashlight-smartled` in the firmware.
- The skirt that sits on the meter matches one specific meter model, so check it against yours before printing.
