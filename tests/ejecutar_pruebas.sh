# Script de pruebas para el proyecto 

set -u

DIR_SCRIPT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DIR_RAIZ="$( cd "${DIR_SCRIPT}/.." && pwd )"
BIN="${DIR_RAIZ}/bin/jqindex"

ROJO=$'\033[0;31m'
VERDE=$'\033[0;32m'
AMARILLO=$'\033[0;33m'
RESET=$'\033[0m'

pruebas_totales=0
pruebas_aprobadas=0
pruebas_fallidas=0

verificar_igual() {
    local descripcion="$1"
    local obtenido="$2"
    local esperado="$3"

    pruebas_totales=$((pruebas_totales + 1))
    if [[ "${obtenido}" == "${esperado}" ]]; then
        echo "  ${VERDE}OK${RESET}   ${descripcion}"
        pruebas_aprobadas=$((pruebas_aprobadas + 1))
    else
        echo "  ${ROJO}FAIL${RESET} ${descripcion}"
        echo "         Esperado: ${esperado}"
        echo "         Obtenido: ${obtenido}"
        pruebas_fallidas=$((pruebas_fallidas + 1))
    fi
}

verificar_contiene() {
    local descripcion="$1"
    local obtenido="$2"
    local esperado="$3"

    pruebas_totales=$((pruebas_totales + 1))
    if [[ "${obtenido}" == *"${esperado}"* ]]; then
        echo "  ${VERDE}OK${RESET}   ${descripcion}"
        pruebas_aprobadas=$((pruebas_aprobadas + 1))
    else
        echo "  ${ROJO}FAIL${RESET} ${descripcion}"
        echo "         Debia contener: ${esperado}"
        echo "         Obtenido:       ${obtenido}"
        pruebas_fallidas=$((pruebas_fallidas + 1))
    fi
}

ejecutar_busqueda() {
    local archivo="$1"
    local expresion="$2"
    "${BIN}" search "${archivo}" "${expresion}"
}

if [[ ! -x "${BIN}" ]]; then
    echo "${AMARILLO}Binario no encontrado. Compilando con make...${RESET}"
    if ! make -C "${DIR_RAIZ}" >/dev/null; then
        echo "${ROJO}Error: la compilacion fallo.${RESET}"
        exit 1
    fi
fi

echo
echo "Batería 1: datos.json (ejemplo del enunciado)"

JSON_DATOS="${DIR_SCRIPT}/datos.json"
"${BIN}" build "${JSON_DATOS}" >/dev/null

INDICE_DATOS="${DIR_SCRIPT}/datos.jnx"
if [[ ! -f "${INDICE_DATOS}" ]]; then
    echo "${ROJO}FAIL${RESET} No se creo el archivo de indice."
    exit 1
fi
echo "  ${VERDE}OK${RESET}   Archivo de indice generado: ${INDICE_DATOS}"
pruebas_totales=$((pruebas_totales + 1))
pruebas_aprobadas=$((pruebas_aprobadas + 1))

verificar_contiene "Indice contiene /usuarios" \
    "$(cat "${INDICE_DATOS}")" "/usuarios"
verificar_contiene "Indice contiene /usuarios/0/nombre" \
    "$(cat "${INDICE_DATOS}")" "/usuarios/0/nombre"
verificar_contiene "Indice contiene /config/opciones/modo" \
    "$(cat "${INDICE_DATOS}")" "/config/opciones/modo"

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$/usuarios/[0-9]+/nombre')
verificar_igual "Busqueda /usuarios/[0-9]+/nombre" \
    "${resultado}" '["Ana", "Luis"]'

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$.*/activo')
verificar_igual "Busqueda .*/activo" \
    "${resultado}" '[true, false]'

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$/usuarios/0/id')
verificar_igual "Busqueda /usuarios/0/id (escalar)" \
    "${resultado}" "1"

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$/config/version')
verificar_igual "Busqueda /config/version (string)" \
    "${resultado}" '"1.0"'

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$/config/opciones')
verificar_contiene "Busqueda /config/opciones devuelve objeto" \
    "${resultado}" '"modo"'

resultado=$(ejecutar_busqueda "${JSON_DATOS}" '$/no/existe')
verificar_igual "Busqueda sin coincidencias devuelve []" \
    "${resultado}" '[]'

echo
echo "Bateria 2: empresa.json (estructura anidada compleja)"

JSON_EMPRESA="${DIR_SCRIPT}/empresa.json"
"${BIN}" build "${JSON_EMPRESA}" >/dev/null

INDICE_EMPRESA="${DIR_SCRIPT}/empresa.jnx"
if [[ ! -f "${INDICE_EMPRESA}" ]]; then
    echo "${ROJO}FAIL${RESET} No se creo empresa.jnx."
    exit 1
fi

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" \
    '$/departamentos/[0-9]+/empleados/[0-9]+/nombre')
verificar_igual "Nombres de todos los empleados" \
    "${resultado}" \
    '["Marta", "Roberto", "Sofia", "Carlos", "Daniela"]'

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$/contacto/.*')
verificar_contiene "Subarbol /contacto contiene telefono" "${resultado}" '"2550-2000"'
verificar_contiene "Subarbol /contacto contiene redes"    "${resultado}" '@TECoficial'

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$.*/activo')
verificar_igual "Todos los activo de empleados" \
    "${resultado}" '[true, true, false, true, true]'

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$/activa')
verificar_igual "Campo raiz activa" "${resultado}" "true"

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$/anio_fundacion')
verificar_igual "Anio de fundacion (numero)" "${resultado}" "1971"

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$/valores')
verificar_igual "Arreglo valores" "${resultado}" \
    '[ "excelencia", "innovacion", "compromiso" ]'

resultado=$(ejecutar_busqueda "${JSON_EMPRESA}" '$/valores/[0-9]+')
verificar_igual "Cada valor por separado" "${resultado}" \
    '["excelencia", "innovacion", "compromiso"]'

echo
echo "======================================================="
echo "  Pruebas totales:    ${pruebas_totales}"
echo "  ${VERDE}Aprobadas:${RESET}          ${pruebas_aprobadas}"
echo "  ${ROJO}Fallidas:${RESET}           ${pruebas_fallidas}"
echo "======================================================="

if [[ ${pruebas_fallidas} -eq 0 ]]; then
    echo "${VERDE}Todas las pruebas pasaron.${RESET}"
    exit 0
else
    echo "${ROJO}Hay pruebas fallidas.${RESET}"
    exit 1
fi
