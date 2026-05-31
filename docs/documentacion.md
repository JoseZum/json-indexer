# Indexado y Búsqueda de Archivos JSON

## Proyecto 2 — Sistemas Operativos

---

## Portada

| Campo | Valor |
|-------|-------|
| **Curso** | Principios de Sistemas Operativos |
| **Proyecto** | Proyecto 2 — Indexado de Archivos JSON |
| **Institución** | Instituto Tecnológico de Costa Rica |
| **Carrera** | Ingeniería en Computación |
| **Semestre** | V semestre |
| **Autores** | José Fabián Zumbado Ruiz |
|              | Fabricio Herrera Rodríguez |
| **Lenguajes** | C (librería) y C++ (CLI) |
| **Sistema objetivo** | Linux (Ubuntu) y WSL sobre Windows |

---

## 1. Introducción

Los sistemas operativos tradicionales incluían entre sus servicios la gestión
de archivos indexados (ISAM), donde un archivo de registros binarios se
acompañaba de un archivo de índice que permitía acceder rápidamente a cada
registro. Hoy esa funcionalidad ya no forma parte del núcleo del sistema
operativo y los datos se almacenan habitualmente en archivos JSON de texto
legible. Aun así, la idea de indexado sigue siendo valiosa: para archivos
JSON grandes, recorrer todo el contenido cada vez que se necesita consultar
una porción es costoso.

Este proyecto retoma el concepto de archivos indexados y lo adapta al
ecosistema JSON. Se desarrolla una librería en C que recorre una sola vez el
archivo JSON, registra dónde se encuentra cada valor (en bytes) y guarda
esa información en un archivo de índice independiente con extensión
`.jnx`. Posteriormente, cualquier consulta se resuelve aplicando una
expresión regular sobre las rutas almacenadas en el índice y leyendo
directamente del archivo original mediante `fseek` solo los bytes
correspondientes a los valores que coinciden.

---

## 2. Análisis del Problema

### 2.1 Requerimientos funcionales

- **R1.** Construir un índice (`.jnx`) a partir de un archivo JSON bien
  formado, almacenando para cada elemento la ruta JSON Pointer y las
  posiciones inicial y final en bytes.
- **R2.** Permitir búsquedas en el índice mediante expresiones regulares
  POSIX extendidas; todas las expresiones inician con `$`, que representa
  la raíz del documento.
- **R3.** Recuperar de forma directa los fragmentos del JSON original
  mediante `fseek` y `fread`, sin volver a parsear el archivo completo.
  Esta operación se expone también como función pública independiente
  (`extraer_fragmento`) además de ser utilizada internamente por la
  búsqueda.
- **R4.** Devolver un arreglo JSON cuando haya varias coincidencias y el
  valor escalar tal cual cuando la coincidencia sea única.
- **R5.** Exponer la funcionalidad como una librería en C
  (`libjsonindex.h`/`libjsonindex.a`) y un programa de línea de comandos
  (`jqindex`). La API pública incluye las tres funciones solicitadas
  por el enunciado:

  | Función | Propósito |
  |---------|-----------|
  | `generar_indice`     | Construye el archivo `.jnx` a partir del JSON. |
  | `buscar_en_indice`   | Aplica la regex sobre las rutas del índice y devuelve el JSON resultado. |
  | `extraer_fragmento`  | Extrae con `fseek`+`fread` los bytes `[inicio, fin)` del JSON original. |

### 2.2 Requerimientos no funcionales

- **N1.** Sin librerías externas: el parseo de JSON, la lectura/escritura
  del índice y la búsqueda se implementan únicamente con la biblioteca
  estándar de C y `regex.h` de POSIX.
- **N2.** Código limpio, con identificadores y comentarios en español.
- **N3.** Portabilidad a Linux (Ubuntu) y a WSL sobre Windows.
- **N4.** Buenas prácticas: separación entre cabecera e implementación,
  enums para códigos de error, validación de retornos de `malloc` y `fopen`,
  liberación correcta de recursos, compilación con `-Wall -Wextra -Wpedantic`.

### 2.3 Restricciones

- El JSON de entrada se asume bien formado.
- Se debe escribir un parser quick-and-dirty propio.
- La extensión del archivo de índice debe ser `.jnx` y su formato debe
  estar documentado.
- Las expresiones de búsqueda deben empezar con `$`.

---

## 3. Solución Propuesta

### 3.1 Arquitectura general

El sistema se divide en dos componentes principales:

```
+--------------------------------------+
|              jqindex (C++)           |     CLI
+--------------------------------------+
                   |
                   v
+--------------------------------------+
|     libjsonindex (librería en C)     |     Librería
|  - Parser JSON                       |
|  - Generación de índice .jnx         |
|  - Carga de índice                   |
|  - Búsqueda con regex.h              |
|  - Extracción con fseek/fread        |
+--------------------------------------+
```

`jqindex` despacha los subcomandos `build` y `search` a las funciones
correspondientes de la librería y formatea las salidas para la terminal.
La librería contiene toda la lógica de indexado y búsqueda; puede
reutilizarse desde otros programas en C o C++.

### 3.2 Estructuras de datos

```c
typedef struct {
    char *ruta;     /* Ruta JSON Pointer del elemento. */
    long  inicio;   /* Byte inicial del valor en el JSON original. */
    long  fin;      /* Byte final exclusivo. */
} EntradaIndice;

typedef struct {
    EntradaIndice *entradas;
    size_t         cantidad;
    size_t         capacidad;
} Indice;
```

El índice es un arreglo dinámico que duplica su capacidad cuando se llena,
manteniendo un costo amortizado O(1) por inserción.

### 3.3 Parser JSON

El parser opera sobre un buffer cargado completamente en memoria. Esto
permite calcular posiciones absolutas exactas y simplifica el manejo de
estados. La función central es:

```c
static int parsear_valor(EstadoParseador *estado, const char *ruta_padre);
```

`parsear_valor` se invoca recursivamente y maneja los seis tipos de valor
JSON:

| Tipo    | Reconocedor inicial | Acción |
|---------|---------------------|--------|
| Objeto  | `{`                 | Reserva entrada en pre-orden, recorre pares `clave:valor` y recurse sobre cada valor. |
| Arreglo | `[`                 | Reserva entrada en pre-orden y recurse sobre cada elemento usando el índice numérico como segmento de ruta. |
| String  | `"`                 | Avanza hasta la comilla de cierre, respetando escapes. |
| Número  | `-`, `0-9`          | Consume dígitos, signo, punto decimal y exponente. |
| `true`/`false`/`null` | `t`/`f`/`n` | Compara contra el literal exacto. |

Para que el índice quede en **pre-orden** (padres antes que hijos, como
en el ejemplo del enunciado), las entradas correspondientes a objetos y
arreglos se reservan justo antes de descender en sus hijos; el campo `fin`
se actualiza cuando el parser regresa de la subllamada. Esto evita realizar
una segunda pasada y mantiene tiempo lineal sobre la entrada.

### 3.4 Formato del archivo `.jnx`

El archivo de índice es texto plano ASCII/UTF-8 con dos partes:

1. **Encabezado** de cinco líneas iniciadas con `#`, donde se documentan
   el formato, el archivo de origen y la cantidad de entradas.
2. **Cuerpo**: una entrada por línea con tres campos separados por TAB
   (`\t`):

```
<ruta>\t<inicio>\t<fin>\n
```

Donde:

- `ruta` es la ruta absoluta tipo JSON Pointer (por ejemplo
  `/usuarios/0/nombre`).
- `inicio` es la posición en bytes del primer carácter del valor en el
  archivo JSON original (incluyendo `"`, `{` o `[` si aplica).
- `fin` es la posición inmediatamente posterior al último carácter del
  valor (exclusiva). El tamaño del valor es siempre `fin - inicio`.

El formato fue elegido por su simplicidad: es legible, se puede inspeccionar
con cualquier editor de texto y es trivial de parsear con `fgets` +
`strchr('\t')`. Las líneas que inician con `#` se ignoran al cargar, lo
que permite agregar comentarios futuros sin romper compatibilidad.

### 3.5 Búsqueda con expresiones regulares

La función `buscar_en_indice` recibe la expresión, valida que empiece con
`$`, descarta ese prefijo y construye un patrón anclado con `^` al inicio
y `$` al final. Por ejemplo:

| Expresión usuario              | Patrón compilado          |
|--------------------------------|----------------------------|
| `$/usuarios/[0-9]+/nombre`     | `^/usuarios/[0-9]+/nombre$`|
| `$/config/.*`                  | `^/config/.*$`             |
| `$.*/activo`                   | `^.*/activo$`              |

El patrón se compila con `regcomp` usando `REG_EXTENDED`. Luego se itera
sobre todas las entradas del índice y se registran los que cumplen
`regexec == 0`. Para cada coincidencia se hace `fseek` al `inicio`
correspondiente y `fread` de `fin - inicio` bytes.

### 3.6 Formato de salida

- **Sin coincidencias:** se devuelve la cadena `[]`.
- **Una coincidencia:** se devuelve el valor tal cual aparece en el JSON
  (string entre comillas, objeto, arreglo, número, booleano o `null`).
- **Múltiples coincidencias:** se devuelve un arreglo JSON en el que cada
  elemento es el fragmento copiado del archivo original, separados por
  `, `. El orden preserva el del índice, que está en pre-orden DFS.

### 3.7 Interfaz de línea de comandos

```text
Uso:
  jqindex build  <archivo.json>
  jqindex search <archivo.json> "<expresion>"
```

El programa deriva la ruta del índice reemplazando la extensión del
archivo JSON por `.jnx`. Devuelve `0` en éxito y `1` en error, y escribe
los mensajes de diagnóstico a `stderr` y los resultados a `stdout`.

---

## 4. Ejecución de Pruebas

### 4.1 Entorno

- Distribución: Ubuntu 24.04 (sobre WSL2 en Windows 11).
- Compilador: `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`.
- Banderas de compilación: `-Wall -Wextra -Wpedantic -O2 -std=c11`
  para C y `-std=c++11` para C++.

### 4.2 Construcción

```bash
make clean && make all
```

Genera dos artefactos:

- `build/libjsonindex.a` — librería estática.
- `bin/jqindex` — programa de línea de comandos.

### 4.3 Batería de pruebas automatizada

Se ejecuta con `make test` o `bash tests/ejecutar_pruebas.sh`. Las pruebas
cubren dos archivos:

#### 4.3.1 `tests/datos.json`

Reproduce el ejemplo del enunciado del proyecto:

```json
{
  "usuarios": [
    { "id": 1, "nombre": "Ana",  "activo": true  },
    { "id": 2, "nombre": "Luis", "activo": false }
  ],
  "config": {
    "version": "1.0",
    "opciones": { "modo": "auto" }
  }
}
```

Casos verificados:

| # | Comando | Resultado esperado |
|---|---------|---------------------|
| 1 | `jqindex build datos.json` | Crea `datos.jnx` con 13 entradas |
| 2 | El índice contiene `/usuarios`, `/usuarios/0/nombre` y `/config/opciones/modo` | OK |
| 3 | `search "$/usuarios/[0-9]+/nombre"` | `["Ana", "Luis"]` |
| 4 | `search "$.*/activo"` | `[true, false]` |
| 5 | `search "$/usuarios/0/id"` | `1` |
| 6 | `search "$/config/version"` | `"1.0"` |
| 7 | `search "$/config/opciones"` | Objeto con `"modo"` |
| 8 | `search "$/no/existe"` | `[]` |

#### 4.3.2 `tests/empresa.json`

Estructura más profunda con dos departamentos, varios empleados y datos
de contacto anidados. Casos verificados:

| # | Comando | Resultado esperado |
|---|---------|---------------------|
| 1 | Nombres de todos los empleados | `["Marta", "Roberto", "Sofia", "Carlos", "Daniela"]` |
| 2 | Subárbol `/contacto` | Contiene `"2550-2000"` y `@TECoficial` |
| 3 | Todos los `activo` de empleados | `[true, true, false, true, true]` |
| 4 | `$/activa` (campo raíz) | `true` |
| 5 | `$/anio_fundacion` | `1971` |
| 6 | `$/valores` | Arreglo completo |
| 7 | `$/valores/[0-9]+` | Cada elemento por separado |

### 4.4 Resultado

```
=======================================================
  Pruebas totales:    18
  Aprobadas:          18
  Fallidas:           0
=======================================================
Todas las pruebas pasaron.
```

---

## 5. Análisis de Resultados

### 5.1 Corrección

Las 18 pruebas automatizadas pasan tanto en Ubuntu nativo como en WSL.
Las salidas para el ejemplo del enunciado coinciden con lo esperado por
el documento original (`["Ana", "Luis"]` para
`$/usuarios/[0-9]+/nombre`, etc.).

### 5.2 Verificación manual del índice generado

Para `datos.json`, el índice queda exactamente en el orden del ejemplo del
enunciado:

```
/usuarios            16   166
/usuarios/0          22    88
/usuarios/0/id       36    37
/usuarios/0/nombre   55    60
/usuarios/0/activo   78    82
/usuarios/1          94   162
...
/config             180   252
/config/version     197   202
/config/opciones    220   248
/config/opciones/modo 236  242
```

Las posiciones en bytes difieren de las del enunciado porque dependen de
la indentación exacta del JSON; sin embargo el rango (`fin - inicio`) de
cada valor es coherente con su contenido. Por ejemplo,
`/usuarios/0/nombre` cubre 5 bytes (`60 - 55 = 5`), que es exactamente la
longitud de `"Ana"` incluyendo las comillas.

### 5.3 Complejidad

- **Construcción del índice:** `O(n)` en el tamaño del JSON, ya que el
  parser realiza una sola pasada. La inserción amortizada en el arreglo
  dinámico es `O(1)`.
- **Carga del índice:** `O(m)` donde `m` es el número de entradas.
- **Búsqueda:** `O(m · |patrón|)` en el peor caso por la evaluación de
  la regex. Cada extracción cuesta `O(1)` en operaciones de archivo más
  el tamaño del fragmento leído.

### 5.4 Limitaciones conocidas

1. El archivo JSON se carga completo en memoria durante la fase de
   indexado. Para archivos verdaderamente enormes (decenas de gigabytes)
   sería preferible usar `mmap` o procesamiento por bloques.
2. El parser asume JSON bien formado; no produce mensajes de diagnóstico
   detallados cuando hay errores de sintaxis, solo retorna
   `ERROR_JSON_INVALIDO`.
3. La longitud máxima de una ruta es `LONGITUD_MAX_RUTA` (4096 bytes), lo
   que cubre con holgura cualquier caso razonable pero es un tope fijo.

---

## 6. Conclusiones

- **El paradigma ISAM sigue siendo útil.** Aplicarlo a archivos JSON
  evita parsear repetidamente documentos grandes y permite responder
  consultas en tiempo proporcional al resultado, no al tamaño total del
  archivo.
- **Los archivos `.jnx` en texto plano** son una solución pragmática:
  son fáciles de inspeccionar, se generan rápido y se cargan con muy
  pocas líneas de código, sin sacrificar funcionalidad. La separación
  entre encabezado comentado y cuerpo TSV facilita la evolución del
  formato.
- **`regex.h` cubre todos los casos del enunciado.** Compilar con
  `REG_EXTENDED` y anclar con `^...$` da semántica intuitiva al usuario:
  las expresiones se leen como rutas con comodines, no como búsquedas
  parciales.
- **El parser quick-and-dirty es suficiente** para JSON bien formado.
  Implementarlo desde cero refuerza la comprensión de cómo
  los descenders recursivos interactúan con buffers y posiciones en
  bytes.
- **El proyecto demuestra los conceptos centrales del curso:** acceso
  directo a posiciones de archivo (`fseek`/`fread`), separación entre
  representación lógica y física de los datos, gestión cuidadosa de
  memoria dinámica y diseño de un formato de archivo propio.
- **La portabilidad Ubuntu/WSL** se obtuvo con disciplina mínima:
  apertura de archivos en modo binario para evitar conversiones de fin
  de línea, eliminación de `\r` al cargar el índice y uso exclusivo de
  APIs POSIX disponibles en ambos entornos.
