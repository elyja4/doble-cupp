# 2far doblecupp — VST3 (JUCE / C++)

Un vaso de líquido morado que se inclina y se derrama sobre tu audio. Usa tu imagen de anime
como póster en la pared (`Assets/character.png`, incrustada en el plugin sin modificar).

## Qué hace el audio (real, procesado en tiempo real)
SPILL controla cuánto líquido se ha derramado. Cuanto más se derrama, más "sumergido" suena:
- Filtro paso bajo que baja de ~20 kHz a ~350 Hz (con resonancia creciente)
- Vibrato líquido (profundidad ±2.5 ms y velocidad 0.8–4 Hz crecen con el derrame)
- Pérdida de nivel de hasta -6 dB
- MIX mezcla con la señal seca (latencia fija de 4 ms, informada al host)

## Controles (todos automatizables)
| Control | Función |
|---|---|
| SPILL (0–100) | Inclina el vaso y derrama el líquido |
| AMOUNT | Cuánto del líquido puede llegar a salir (limita el derrame máximo) |
| SPEED | Velocidad de respuesta de la animación **y** del audio (0.25x–4x) |
| CUP FILL (1–16 oz) | Líquido que hay en el vaso al principio |
| MIX | Dry/Wet |

Lecturas: IN THE CUP (oz restantes), SPILLED (% derramado), INTENSITY (derrame × mix, también en la barra LED).
La animación reacciona al audio: el líquido se bambolea con el nivel, el charco ondula y salen gotas con los transientes.
Presets de fábrica: selector en la esquina superior izquierda (y como programas del host).

## Compilar en Windows
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Resultado: `build\Doblecupp_artefacts\Release\VST3\2far doblecupp.vst3`
(O deja que GitHub Actions lo compile: `.github/workflows/build.yml`.)

## Instalar
1. Copia la carpeta `2far doblecupp.vst3` a `C:\Program Files\Common Files\VST3\` (administrador).
2. FL Studio: Options > Manage plugins > Find installed plugins.
3. Slot FX del mezclador > Add > *2far doblecupp*.

AU: el CMake lo compila solo en macOS. No se puede generar desde Windows.
