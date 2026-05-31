# json-indexer

Indexado y búsqueda de archivos JSON mediante expresiones regulares.

**Proyecto 2: Sistemas Operativos**
Autores: José Fabián Zumbado Ruiz y Cesar Fabricio Herrera Rodríguez.

---

## Descripción

`json-indexer` "aplana" un archivo JSON y construye un índice
(`.jnx`) que registra para cada elemento su ruta tipo JSON Pointer
junto con la posición en bytes donde inicia y termina su valor. Una vez
construido el índice, cualquier consulta se resuelve aplicando una
expresión regular POSIX sobre las rutas y leyendo directamente del
archivo original con `fseek`/`fread`, sin volver a parsearlo.

El proyecto entrega:

- `libjsonindex.h` / `libjsonindex.a` — librería en C reutilizable.
- `jqindex` — programa de línea de comandos en C++.

No se utilizan dependencias externas: el parser JSON, la serialización
del índice y la búsqueda están implementados solo con la biblioteca
estándar de C y `regex.h` de POSIX.

---

## Estructura del proyecto

```
json-indexer/
├── include/
│   └── libjsonindex.h          # API pública de la librería
├── src/
│   ├── libjsonindex.c          # Implementación (parser, índice, búsqueda)
│   └── jqindex.cpp             # Programa de línea de comandos
├── tests/
│   ├── datos.json              # Ejemplo del enunciado
│   ├── empresa.json            # JSON anidado complejo
│   └── ejecutar_pruebas.sh     # Batería de 18 pruebas automatizadas
├── docs/
│   └── documentacion.md        # Documentación formal completa
├── Makefile
└── README.md
```

---

## Compilación

### Requisitos

- `gcc` y `g++` (C11 / C++11).
- `make`.
- Sistema POSIX (Ubuntu, Debian, etc.) o WSL sobre Windows.

### Pasos

```bash
make clean && make all
```

Esto produce:

- `build/libjsonindex.a` — librería estática.
- `bin/jqindex` — binario del programa de línea de comandos.

---

## Uso

### Construir el índice

```bash
bin/jqindex build tests/datos.json
```

Genera `tests/datos.jnx`.

### Buscar con expresiones regulares

Toda expresión debe iniciar con `$`, que representa la raíz del JSON.

```bash
bin/jqindex search tests/datos.json "$/usuarios/[0-9]+/nombre"
# -> ["Ana", "Luis"]

bin/jqindex search tests/datos.json "$/config/.*"
# -> ["1.0", { "modo": "auto" ... }, "auto"]

bin/jqindex search tests/datos.json "$.*/activo"
# -> [true, false]

bin/jqindex search tests/datos.json "$/usuarios/0/id"
# -> 1
```

Reglas de salida:

- Sin coincidencias → `[]`.
- Una coincidencia → el valor tal cual aparece en el JSON.
- Múltiples coincidencias → arreglo JSON en pre-orden DFS.

---

## Pruebas

```bash
make test
```

Ejecuta `tests/ejecutar_pruebas.sh`, que recompila si es necesario y
corre 18 pruebas que cubren el ejemplo del enunciado y un JSON anidado
más complejo. Una salida exitosa termina con:

```
=======================================================
  Pruebas totales:    18
  Aprobadas:          18
  Fallidas:           0
=======================================================
Todas las pruebas pasaron.
```

---

## Formato del archivo `.jnx`

Texto plano. Cinco líneas de encabezado iniciadas con `#` y luego una
entrada por línea con tres campos separados por TAB:

```
<ruta>\t<inicio>\t<fin>\n
```

- `ruta` — ruta absoluta JSON Pointer (`/usuarios/0/nombre`).
- `inicio` — byte inicial del valor (incluyendo `"`, `{` o `[`).
- `fin` — byte inmediatamente posterior al valor (exclusivo).

Las líneas que inician con `#` se ignoran al cargar.

---

## API de la librería

```c
#include "libjsonindex.h"

/* Las tres funciones principales solicitadas por el enunciado: */
int   generar_indice  (const char *ruta_json, const char *ruta_indice);
char *buscar_en_indice(const char *ruta_json,
                       const char *ruta_indice,
                       const char *expresion);
char *extraer_fragmento(const char *ruta_json, long inicio, long fin);

/* Utilidades auxiliares para manejar el índice en memoria: */
Indice *cargar_indice (const char *ruta_indice);
void    liberar_indice(Indice *indice);
```

Para usarla desde C++ basta incluir el mismo encabezado; el bloque
`extern "C"` está provisto.

---

## Probar en Ubuntu nativo

```bash
sudo apt update
sudo apt install build-essential
cd json-indexer
make clean && make test
```

## Probar en WSL (desde Windows)

```powershell
wsl
cd "/mnt/c/Users/<usuario>/.../Proyecto2/json-indexer"
make clean && make test
```

---

## Documentación formal

El informe completo (portada, introducción, análisis del problema,
solución propuesta, pruebas, análisis y conclusiones) está en /docs.
