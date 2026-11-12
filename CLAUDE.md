# West Coast Modular — lo que hay que saber antes de tocar nada

Rack modular de la costa oeste, JUCE 8 + CMake, todo adentro de un solo plugin.
Marca: **Calabi Yau**. Idioma de trabajo con Martin: **castellano rioplatense, de vos**.

Este archivo junta lo que costó descubrir. Leerlo entero antes de empezar sale más barato que
redescubrirlo: casi todo lo que está acá se pagó con un bug en producción o con medio día de
compilaciones.

**Este repositorio es `Calabiyau20/gulari` y es público.** Dos plugins viven acá: **West Coast
Modular** (el rack, `Modular/`) y **Synthgularity** (el sinte con oscilador granular). Por eso el
directorio de build se llama `Synthgularity`.

Como el repo es público, acá no va nada de la operación del negocio: ni cómo se emiten las
licencias, ni qué productos están en camino, ni ninguna credencial. Eso está documentado aparte,
en el repositorio privado de licencias. Lo único de licencias que corresponde acá es cómo las
**valida** el plugin, que está más abajo y no es secreto: lleva solo la clave pública.

### Los otros plugins de Calabi Yau

Existen y comparten marca, manual de estilo y criterios de interfaz, pero cada uno tiene su propio
repositorio y su propio contexto. Si el trabajo es sobre uno de ellos, este archivo no alcanza:
**Calabi-Yau Resonator** y **The SingulariBass**.

Lo que sí vale para todos: la interfaz sigue el manual de marca Calabi Yau (paleta Cosmos,
Syncopate y Montserrat, marcos biselados), y **el isologo es siempre el PNG oficial, nunca el
nombre escrito con una fuente**.

---

## Compilar y probar

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPG_BUILD_TESTS=ON
cmake --build build --target WcRenderTest WcUiSnapshot
./build/WcRenderTest_artefacts/Release/WcRenderTest  <dir>      # DSP, módulos, licencias
xvfb-run -a ./build/WcUiSnapshot_artefacts/Release/WcUiSnapshot <dir>   # paneles, zoom, cables, botones
```

- **`PG_BUILD_TESTS` está en OFF por defecto.** Sin ese flag no existen los targets de prueba.
- Los dos tests devuelven 1 si algo falla e imprimen `ALL OK` / `FAILED`.
- El CI (`.github/workflows/build.yml`) corre los dos en Ubuntu y **macOS y Windows dependen de
  ese job**: si un test falla no se empaqueta nada. Los runners de Mac y Windows no tienen
  servidor gráfico, por eso el test de interfaz corre solo en Linux.

Otros targets: `WcPresetGen` (la librería de fábrica), `WcPackGen` (packs de presets),
`WcKeygen` (claves de licencia).

**Ground truth de los parámetros**, en vez de adivinar leyendo `WcParams.cpp`:

```bash
./build/WcPackGen_artefacts/Release/WcPackGen --params
```

---

## El rack

Ranuras en `Modular/Source/rack/Rack.h`: `kInstMidi`, `kInst218`, `242`, `245`, `248`, `250`,
`252`, `266`, `230`, `264`, `257`, `281` (×2), `284`, `258` (×2), `259` (×4), `285`, `267`,
`291`, `296`, `277`, `292` (×2), `275`, `207`, `666` (×4), `kInstOutput`.

Prefijo de parámetros e `instKey` por ranura — **no son el número del módulo**:

| módulo | key / prefijo | módulo | key / prefijo |
|---|---|---|---|
| 218 | `kb_` | 267 | `us_` |
| 242 | `pp_` | 275 | `rv_` |
| 245 | `sv_` | 284 | `ev_` |
| 248 | `mf_` | 291 | `tm_` |
| 250 | `sq_` | 296 | `sp_` |
| 252 | `rg_` | 277 | `dl_` |
| 266 | `ua_` | 281 A/B | `fa_` / `fb_` |
| 230 | `ef_` | 258 A/B | `oa_` / `ob_` |
| 264 | `sh_` | 259 A-D | `a_` `b_` `c_` `d_` |
| 257 | `vp_` | 292 A/B | `la_` / `lb_` |
| 285 | `fs_` | 666 ×4 | `s1_`…`s4_` |
| 207 | `mx_` | salida | `out_` |

Helpers: `prefix259(i)`, `prefix281(i)`, `prefix292(i)`, `prefix258(i)`, `prefix666(i)`.

### Cables

- **Un cable nuevo a una entrada ya ocupada REEMPLAZA al anterior.** Igual que en el hardware:
  entra un plug por jack. Mandar cuatro voces a `In_Left`/`In_Right` deja oír dos. Para sumar
  varias fuentes hay que pasar por el **mixer 207**, que tiene seis entradas.
- `connect()` **rechaza** si no coinciden los tipos. Audio = jack mini; c.v. y pulsos = banana.
  Los tipos salen del panel (`Specs.cpp`), no del enum. Devuelve `false`, no tira excepción:
  conviene mirar el valor de retorno.
- Jacks por función, no por índice: `M292::inSig/inCtl/outSig(lane)`, `M281::inTrig/outFunc(lane)`,
  `M284::inTrig/outEnv(ch, i)`, `M248::inJ(k, n)`.

---

## Trampas de nombres de parámetros

Las que costaron compilaciones. Ante la duda, `--params`.

| módulo | es | **no** es |
|---|---|---|
| 292 | `mode`, `cv`, `off`, `res` | `level` |
| 281 | `att`, `dec`, `mode` (0 transitorio, 1 sostenido, **2 ciclo**) | `cycle` |
| 258 | `shape1`, `shape2` | `wave` |
| 285 | `fs_ctl`, `fs_amp` | `fs_shift`, `fs_mix` |
| 277 | `dl_time`, `dl_mod`, `dl_lvla` | `dl_fb`, `dl_mix` |
| 207 | `mx_lvl_a`…`_f`, `mx_prog_a`…, `mx_pan1`…`pan4` | `mx_pan_a` |
| 266 | `ua_rate` (el primero **sin número**), `ua_rate2`… | `ua_rate1` |
| 242 | `pp_a1`…`a12`, `pp_b*`, `pp_c*`, `pp_len`, `pp_period` | `pp_rate` |
| 245 | `sv_time`, `sv_len`, `sv_a1`…`a5`, `sv_b*`… | `sv_rate` |

---

## Comportamientos que no se ven leyendo el código

**Modo demo.** Sin archivo de licencia válido, `setPresent()` **rechaza en silencio** el tercer y
cuarto oscilador (`License::kDemoMaxOscillators`) y `savePatchFile()` devuelve false. Por eso la
librería de 300 presets de fábrica no usa nunca el 259 C ni el D: el generador corría sin licencia
y el plugin se los negaba sin avisar. **Cualquier herramienta que arme patches tiene que correr
licenciada** — ver `WcPackGen`, que firma una clave temporal y llama a `recheckLicense()`.

**252 — las celdas no son parámetros.** Viven en `rgb[]`, `cv[][]`, `sub[]`, `offset[]` adentro del
módulo (públicos) y se guardan aparte con `toTree`/`fromTree`. Arrancan vacías: si no se programan,
los tres relojes giran sobre nada y no sale un solo pulso. `rgb[i]` es una máscara de bits:
1 rojo, 2 verde, 4 azul. El índice de celda es `M252::base(ring) + cell`, con
`kSize[] = {3,4,5,6,7,8,9,10,11,12,16}`. `rg_clkN` elige el anillo (`clk - 1`).
`rg_sync = 1` mete cada anillo en el mismo ciclo (polirritmia); `= 0` le da a cada celda un beat
(polimetría).

**248 — arranca detenido y se para solo.** Necesita un flanco en `In_StartN` para empezar, y al
llegar a su última etapa se detiene (`atEnd`). Un pulsador conectado a `In_Start1`/`In_Start2` lo
arranca y lo relanza cada vez que termina.

**250 — EDIT es una luz, no una traba.** Decide qué muestran los LEDs de programación: la etapa que
se edita o la que suena. Los interruptores programan la etapa elegida con la perilla del centro
esté encendida o no. Hubo una versión donde EDIT apagaba trece botones; no volver ahí.
El modo `pulse` es el quinto de la fila de cuatro luces: se muestra encendiendo `adv` + `stop`.

**259 — las entradas de c.v. aceptan 0-10 V y el extremo apaga la voz.** Mandarle diez voltios
enteros a `In_TimbreCv` o `In_IndexCv` deja la voz en una punta y casi no sale señal. Atenuar con
el **257** (`vp_k*`, `vp_off*`). La salida de audio es `Out_Final1`.

---

## Archivos de patch y presets

- Patches del usuario: `.wcpatch` en `~/Documents/Calabi Yau/West Coast Modular/Patches/`
  (Windows: `Documentos\Calabi Yau\West Coast Modular\Patches\`). Se pueden copiar a mano.
- Librería de fábrica: `Modular/Presets/factory.xml`, embebida en el binario.
- Formato: `WCSTATE` con el estado del apvts + `WCPATCH` (`MODULES`, `CABLES`) + `RG252`.

---

## Licencias

- Archivo `WestCoastModular.key`: datos del comprador + SHA-256 + firma RSA. **Offline, siempre.**
- El plugin lleva solo la clave **pública**, en `WcLicense.cpp`. Busca el archivo en Documents,
  Application Support, al lado del plugin y en la carpeta del usuario.
- **La clave privada no va nunca al repositorio**, ni como constante, ni en un workflow, ni en un
  archivo de ejemplo. Vive en el gestor de secretos de quien emite.
- El generador en Python (`wc-keygen.py`, repo privado aparte) produce firmas **byte por byte
  idénticas** a `WcKeygen`. La firma es sobre
  `producto\nver\nnombre\nmail\norden\nemitida`, hasheado y leído como entero **little-endian**.

---

## Marca

Ningún panel, ningún string del binario, ningún texto del manual ni de la página de venta puede
nombrar otra marca. Cada layout marca la altura de su pie con un texto `"@foot"` que `finish()`
lee y borra; `RackView` graba ahí `CALABI YAU · isologo · modelo`. Después de tocar `Specs.cpp`:

```bash
strings -a <binario> | grep -ci buchla     # tiene que dar 0
```

El isologo oficial es un PNG. **Nunca se escribe el nombre de la marca con una fuente.**

---

## Al probar

- `WcUiSnapshot` deja un PNG por panel. Compararlos contra una referencia sirve para cazar
  desalineaciones — **pero los paneles 242, 252 y 266 no son determinísticos** entre dos corridas
  del mismo binario: tienen LEDs que parpadean y voltajes al azar. Esos tres no se comparan.
- Un cambio que dice ser solo visual se verifica igual: render de los paneles y diff contra la
  corrida anterior. Cero píxeles de diferencia es la prueba de que no se movió nada.
- Un bug encontrado se arregla **y se le escribe la prueba que lo caza**. Para saber que la prueba
  sirve, se vuelve a meter el bug a propósito y se confirma que falla.

---

## Cómo trabajar acá

Lo que falló en este proyecto nunca fue falta de análisis: fue falta de verificación. Los errores
caros —trece botones muertos, seis licencias regaladas, dos osciladores que el generador nunca
pudo usar— eran todos invisibles leyendo el código y evidentes corriéndolo.

- Una conversación por entregable, no por especialidad.
- Correr y medir antes de dar algo por hecho.
- Ante una configuración ambigua o a medias, **frenar y avisar**, nunca seguir con el valor más
  permisivo. Una variable de entorno vacía es una variable que no está, no un permiso para no
  filtrar.
