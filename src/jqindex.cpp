#include "libjsonindex.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static void imprimir_uso(const char *nombre_programa)
{
    std::cerr << "Uso:\n"
              << "  " << nombre_programa << " build  <archivo.json>\n"
              << "  " << nombre_programa << " search <archivo.json> \"<expresion>\"\n"
              << "\n"
              << "Comandos:\n"
              << "  build   Genera el archivo de indice .jnx a partir del JSON.\n"
              << "  search  Busca en el indice usando una expresion regular POSIX\n"
              << "          extendida. La expresion debe iniciar con '$' (raiz).\n"
              << "\n"
              << "Ejemplos:\n"
              << "  " << nombre_programa << " build datos.json\n"
              << "  " << nombre_programa << " search datos.json \"$/usuarios/[0-9]+/nombre\"\n"
              << "  " << nombre_programa << " search datos.json \"$/config/.*\"\n"
              << "  " << nombre_programa << " search datos.json \"$.*/activo\"\n";
}

static std::string derivar_ruta_indice(const std::string &ruta_json)
{
    size_t posicion_punto = ruta_json.find_last_of('.');
    size_t posicion_separador = ruta_json.find_last_of("/\\");
    if (posicion_punto != std::string::npos &&
        (posicion_separador == std::string::npos ||
         posicion_punto > posicion_separador)) {
        return ruta_json.substr(0, posicion_punto) + ".jnx";
    }
    return ruta_json + ".jnx";
}

static const char *mensaje_error(int codigo)
{
    switch (codigo) {
        case RESULTADO_EXITO:
            return "Operacion exitosa.";
        case ERROR_ARCHIVO_NO_ENCONTRADO:
            return "No se pudo abrir el archivo solicitado.";
        case ERROR_MEMORIA:
            return "Memoria insuficiente.";
        case ERROR_JSON_INVALIDO:
            return "El archivo JSON no esta bien formado.";
        case ERROR_REGEX_INVALIDO:
            return "La expresion regular no es valida.";
        case ERROR_EXPRESION_INVALIDA:
            return "La expresion debe comenzar con '$'.";
        case ERROR_ESCRITURA:
            return "Error al escribir el archivo de indice.";
        default:
            return "Error desconocido.";
    }
}

static int ejecutar_build(const std::string &ruta_json)
{
    std::string ruta_indice = derivar_ruta_indice(ruta_json);

    int codigo = generar_indice(ruta_json.c_str(), ruta_indice.c_str());
    if (codigo != RESULTADO_EXITO) {
        std::cerr << "Error al generar el indice: "
                  << mensaje_error(codigo)
                  << " (codigo " << codigo << ")\n";
        return 1;
    }

    std::cout << "Indice generado correctamente en: " << ruta_indice << "\n";
    return 0;
}

static int ejecutar_search(const std::string &ruta_json,
                           const std::string &expresion)
{
    std::string ruta_indice = derivar_ruta_indice(ruta_json);

    char *resultado = buscar_en_indice(ruta_json.c_str(),
                                       ruta_indice.c_str(),
                                       expresion.c_str());
    if (resultado == NULL) {
        std::cerr << "Error al ejecutar la busqueda. "
                  << "Verifique que el indice exista y que la expresion "
                  << "sea valida y comience con '$'.\n";
        return 1;
    }

    std::cout << resultado << "\n";
    std::free(resultado);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        imprimir_uso(argv[0]);
        return 1;
    }

    std::string comando = argv[1];

    if (comando == "--help" || comando == "-h" || comando == "help") {
        imprimir_uso(argv[0]);
        return 0;
    }

    if (comando == "build") {
        if (argc != 3) {
            imprimir_uso(argv[0]);
            return 1;
        }
        return ejecutar_build(argv[2]);
    }

    if (comando == "search") {
        if (argc != 4) {
            imprimir_uso(argv[0]);
            return 1;
        }
        return ejecutar_search(argv[2], argv[3]);
    }

    std::cerr << "Comando desconocido: " << comando << "\n\n";
    imprimir_uso(argv[0]);
    return 1;
}
