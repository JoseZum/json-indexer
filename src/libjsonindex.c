#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "libjsonindex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <errno.h>

#define CAPACIDAD_INICIAL_INDICE 64

typedef struct {
    const char *buffer;
    long        longitud;
    long        posicion;
    Indice     *indice;
} EstadoParseador;

static Indice *crear_indice_vacio(void)
{
    Indice *indice = (Indice *)malloc(sizeof(Indice));
    if (indice == NULL) {
        return NULL;
    }
    indice->entradas = (EntradaIndice *)malloc(sizeof(EntradaIndice) * CAPACIDAD_INICIAL_INDICE);
    if (indice->entradas == NULL) {
        free(indice);
        return NULL;
    }
    indice->cantidad  = 0;
    indice->capacidad = CAPACIDAD_INICIAL_INDICE;
    return indice;
}

static int agregar_entrada_indice(Indice *indice,
                                  const char *ruta,
                                  long inicio,
                                  long fin)
{
    if (indice->cantidad >= indice->capacidad) {
        size_t nueva_capacidad = indice->capacidad * 2;
        EntradaIndice *nuevas = (EntradaIndice *)realloc(
            indice->entradas, sizeof(EntradaIndice) * nueva_capacidad);
        if (nuevas == NULL) {
            return ERROR_MEMORIA;
        }
        indice->entradas  = nuevas;
        indice->capacidad = nueva_capacidad;
    }

    EntradaIndice *entrada = &indice->entradas[indice->cantidad];
    entrada->ruta = strdup(ruta);
    if (entrada->ruta == NULL) {
        return ERROR_MEMORIA;
    }
    entrada->inicio = inicio;
    entrada->fin    = fin;
    indice->cantidad++;
    return RESULTADO_EXITO;
}

void liberar_indice(Indice *indice)
{
    if (indice == NULL) {
        return;
    }
    for (size_t i = 0; i < indice->cantidad; i++) {
        free(indice->entradas[i].ruta);
    }
    free(indice->entradas);
    free(indice);
}

static void saltar_espacios(EstadoParseador *estado)
{
    while (estado->posicion < estado->longitud) {
        char c = estado->buffer[estado->posicion];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            estado->posicion++;
        } else {
            break;
        }
    }
}

static int parsear_cadena(EstadoParseador *estado,
                          long *inicio_contenido,
                          long *longitud_contenido)
{
    if (estado->posicion >= estado->longitud) {
        return -1;
    }
    if (estado->buffer[estado->posicion] != '"') {
        return -1;
    }
    estado->posicion++;

    long inicio = estado->posicion;
    while (estado->posicion < estado->longitud) {
        char c = estado->buffer[estado->posicion];
        if (c == '\\') {
            estado->posicion += 2;
            if (estado->posicion > estado->longitud) {
                return -1;
            }
        } else if (c == '"') {
            if (inicio_contenido != NULL) {
                *inicio_contenido = inicio;
            }
            if (longitud_contenido != NULL) {
                *longitud_contenido = estado->posicion - inicio;
            }
            estado->posicion++;
            return 0;
        } else {
            estado->posicion++;
        }
    }
    return -1;
}

static int parsear_numero(EstadoParseador *estado)
{
    long inicio = estado->posicion;
    if (estado->posicion < estado->longitud && estado->buffer[estado->posicion] == '-') {
        estado->posicion++;
    }
    while (estado->posicion < estado->longitud) {
        char c = estado->buffer[estado->posicion];
        if ((c >= '0' && c <= '9') || c == '.' ||
            c == 'e' || c == 'E' || c == '+' || c == '-') {
            estado->posicion++;
        } else {
            break;
        }
    }
    return (estado->posicion > inicio) ? 0 : -1;
}

static int parsear_literal(EstadoParseador *estado, const char *literal)
{
    long longitud_literal = (long)strlen(literal);
    if (estado->posicion + longitud_literal > estado->longitud) {
        return -1;
    }
    if (strncmp(&estado->buffer[estado->posicion], literal, (size_t)longitud_literal) != 0) {
        return -1;
    }
    estado->posicion += longitud_literal;
    return 0;
}

static int parsear_valor(EstadoParseador *estado, const char *ruta_padre)
{
    saltar_espacios(estado);
    if (estado->posicion >= estado->longitud) {
        return -1;
    }

    long  inicio_valor = estado->posicion;
    char  primer_caracter = estado->buffer[estado->posicion];
    int   resultado;

    if (primer_caracter == '{') {
        size_t indice_entrada = (size_t)-1;
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, -1) != 0) {
                return ERROR_MEMORIA;
            }
            indice_entrada = estado->indice->cantidad - 1;
        }

        estado->posicion++;
        saltar_espacios(estado);

        while (estado->posicion < estado->longitud &&
               estado->buffer[estado->posicion] != '}') {
            saltar_espacios(estado);

            long inicio_clave    = 0;
            long longitud_clave  = 0;
            if (parsear_cadena(estado, &inicio_clave, &longitud_clave) != 0) {
                return -1;
            }

            char nueva_ruta[LONGITUD_MAX_RUTA];
            int  longitud_padre = (int)strlen(ruta_padre);
            if ((long)longitud_padre + longitud_clave + 2 >= LONGITUD_MAX_RUTA) {
                return -1;
            }
            memcpy(nueva_ruta, ruta_padre, (size_t)longitud_padre);
            nueva_ruta[longitud_padre] = '/';
            memcpy(&nueva_ruta[longitud_padre + 1],
                   &estado->buffer[inicio_clave],
                   (size_t)longitud_clave);
            nueva_ruta[longitud_padre + 1 + longitud_clave] = '\0';

            saltar_espacios(estado);
            if (estado->posicion >= estado->longitud ||
                estado->buffer[estado->posicion] != ':') {
                return -1;
            }
            estado->posicion++;
            saltar_espacios(estado);

            resultado = parsear_valor(estado, nueva_ruta);
            if (resultado != 0) {
                return resultado;
            }

            saltar_espacios(estado);
            if (estado->posicion < estado->longitud &&
                estado->buffer[estado->posicion] == ',') {
                estado->posicion++;
                saltar_espacios(estado);
            }
        }

        if (estado->posicion >= estado->longitud) {
            return -1;
        }
        estado->posicion++;

        if (indice_entrada != (size_t)-1) {
            estado->indice->entradas[indice_entrada].fin = estado->posicion;
        }

    } else if (primer_caracter == '[') {
        size_t indice_entrada = (size_t)-1;
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, -1) != 0) {
                return ERROR_MEMORIA;
            }
            indice_entrada = estado->indice->cantidad - 1;
        }

        estado->posicion++;
        saltar_espacios(estado);

        int indice_arreglo = 0;
        while (estado->posicion < estado->longitud &&
               estado->buffer[estado->posicion] != ']') {
            saltar_espacios(estado);

            char nueva_ruta[LONGITUD_MAX_RUTA];
            int  escritos = snprintf(nueva_ruta, LONGITUD_MAX_RUTA,
                                     "%s/%d", ruta_padre, indice_arreglo);
            if (escritos <= 0 || escritos >= LONGITUD_MAX_RUTA) {
                return -1;
            }

            resultado = parsear_valor(estado, nueva_ruta);
            if (resultado != 0) {
                return resultado;
            }
            indice_arreglo++;

            saltar_espacios(estado);
            if (estado->posicion < estado->longitud &&
                estado->buffer[estado->posicion] == ',') {
                estado->posicion++;
                saltar_espacios(estado);
            }
        }

        if (estado->posicion >= estado->longitud) {
            return -1;
        }
        estado->posicion++;

        if (indice_entrada != (size_t)-1) {
            estado->indice->entradas[indice_entrada].fin = estado->posicion;
        }

    } else if (primer_caracter == '"') {
        if (parsear_cadena(estado, NULL, NULL) != 0) {
            return -1;
        }
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, estado->posicion) != 0) {
                return ERROR_MEMORIA;
            }
        }

    } else if (primer_caracter == 't') {
        if (parsear_literal(estado, "true") != 0) {
            return -1;
        }
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, estado->posicion) != 0) {
                return ERROR_MEMORIA;
            }
        }

    } else if (primer_caracter == 'f') {
        if (parsear_literal(estado, "false") != 0) {
            return -1;
        }
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, estado->posicion) != 0) {
                return ERROR_MEMORIA;
            }
        }

    } else if (primer_caracter == 'n') {
        if (parsear_literal(estado, "null") != 0) {
            return -1;
        }
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, estado->posicion) != 0) {
                return ERROR_MEMORIA;
            }
        }

    } else if (primer_caracter == '-' ||
               (primer_caracter >= '0' && primer_caracter <= '9')) {
        if (parsear_numero(estado) != 0) {
            return -1;
        }
        if (ruta_padre[0] != '\0') {
            if (agregar_entrada_indice(estado->indice, ruta_padre,
                                       inicio_valor, estado->posicion) != 0) {
                return ERROR_MEMORIA;
            }
        }

    } else {
        return -1;
    }

    return 0;
}

int generar_indice(const char *ruta_json, const char *ruta_indice)
{
    FILE *fp_json = fopen(ruta_json, "rb");
    if (fp_json == NULL) {
        return ERROR_ARCHIVO_NO_ENCONTRADO;
    }

    if (fseek(fp_json, 0, SEEK_END) != 0) {
        fclose(fp_json);
        return ERROR_ARCHIVO_NO_ENCONTRADO;
    }
    long tamano = ftell(fp_json);
    if (tamano < 0) {
        fclose(fp_json);
        return ERROR_ARCHIVO_NO_ENCONTRADO;
    }
    rewind(fp_json);

    char *buffer = (char *)malloc((size_t)tamano + 1);
    if (buffer == NULL) {
        fclose(fp_json);
        return ERROR_MEMORIA;
    }
    size_t leidos = fread(buffer, 1, (size_t)tamano, fp_json);
    fclose(fp_json);
    if ((long)leidos != tamano) {
        free(buffer);
        return ERROR_ARCHIVO_NO_ENCONTRADO;
    }
    buffer[tamano] = '\0';

    Indice *indice = crear_indice_vacio();
    if (indice == NULL) {
        free(buffer);
        return ERROR_MEMORIA;
    }

    EstadoParseador estado = { buffer, tamano, 0, indice };
    int resultado_parser = parsear_valor(&estado, "");
    if (resultado_parser != 0) {
        free(buffer);
        liberar_indice(indice);
        return ERROR_JSON_INVALIDO;
    }

    FILE *fp_indice = fopen(ruta_indice, "w");
    if (fp_indice == NULL) {
        free(buffer);
        liberar_indice(indice);
        return ERROR_ESCRITURA;
    }

    fprintf(fp_indice, "# Archivo de indice JSON (.jnx)\n");
    fprintf(fp_indice, "# Formato: <ruta>\\t<inicio>\\t<fin>\n");
    fprintf(fp_indice, "# Generado por libjsonindex\n");
    fprintf(fp_indice, "# Archivo origen: %s\n", ruta_json);
    fprintf(fp_indice, "# Cantidad de entradas: %zu\n", indice->cantidad);

    for (size_t i = 0; i < indice->cantidad; i++) {
        fprintf(fp_indice, "%s\t%ld\t%ld\n",
                indice->entradas[i].ruta,
                indice->entradas[i].inicio,
                indice->entradas[i].fin);
    }

    int error_escritura = ferror(fp_indice);
    fclose(fp_indice);
    free(buffer);
    liberar_indice(indice);

    return error_escritura ? ERROR_ESCRITURA : RESULTADO_EXITO;
}

Indice *cargar_indice(const char *ruta_indice)
{
    FILE *fp = fopen(ruta_indice, "r");
    if (fp == NULL) {
        return NULL;
    }

    Indice *indice = crear_indice_vacio();
    if (indice == NULL) {
        fclose(fp);
        return NULL;
    }

    char linea[LONGITUD_MAX_RUTA + 64];
    while (fgets(linea, sizeof(linea), fp) != NULL) {
        if (linea[0] == '#' || linea[0] == '\n' || linea[0] == '\r' ||
            linea[0] == '\0') {
            continue;
        }

        size_t longitud_linea = strlen(linea);
        while (longitud_linea > 0 &&
               (linea[longitud_linea - 1] == '\n' ||
                linea[longitud_linea - 1] == '\r')) {
            linea[--longitud_linea] = '\0';
        }
        if (longitud_linea == 0) {
            continue;
        }

        char *primera_tab = strchr(linea, '\t');
        if (primera_tab == NULL) {
            continue;
        }
        *primera_tab = '\0';

        char *segunda_tab = strchr(primera_tab + 1, '\t');
        if (segunda_tab == NULL) {
            continue;
        }
        *segunda_tab = '\0';

        long inicio = strtol(primera_tab + 1, NULL, 10);
        long fin    = strtol(segunda_tab + 1, NULL, 10);

        if (agregar_entrada_indice(indice, linea, inicio, fin) != 0) {
            liberar_indice(indice);
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);
    return indice;
}

char *extraer_fragmento(const char *ruta_json, long inicio, long fin)
{
    if (ruta_json == NULL || fin < inicio) {
        return NULL;
    }

    FILE *fp_json = fopen(ruta_json, "rb");
    if (fp_json == NULL) {
        return NULL;
    }

    long longitud = fin - inicio;
    char *fragmento = (char *)malloc((size_t)longitud + 1);
    if (fragmento == NULL) {
        fclose(fp_json);
        return NULL;
    }

    if (fseek(fp_json, inicio, SEEK_SET) != 0) {
        free(fragmento);
        fclose(fp_json);
        return NULL;
    }

    size_t leidos = fread(fragmento, 1, (size_t)longitud, fp_json);
    fclose(fp_json);
    if (leidos != (size_t)longitud) {
        free(fragmento);
        return NULL;
    }

    fragmento[longitud] = '\0';
    return fragmento;
}

static char *leer_fragmento_fp(FILE *fp_json, long inicio, long fin)
{
    long longitud = fin - inicio;
    if (longitud < 0) {
        return NULL;
    }
    char *fragmento = (char *)malloc((size_t)longitud + 1);
    if (fragmento == NULL) {
        return NULL;
    }
    if (fseek(fp_json, inicio, SEEK_SET) != 0) {
        free(fragmento);
        return NULL;
    }
    size_t leidos = fread(fragmento, 1, (size_t)longitud, fp_json);
    if ((long)leidos != longitud) {
        free(fragmento);
        return NULL;
    }
    fragmento[longitud] = '\0';
    return fragmento;
}

char *buscar_en_indice(const char *ruta_json,
                       const char *ruta_indice,
                       const char *expresion)
{
    if (expresion == NULL || expresion[0] != '$') {
        return NULL;
    }

    Indice *indice = cargar_indice(ruta_indice);
    if (indice == NULL) {
        return NULL;
    }

    char patron[LONGITUD_MAX_RUTA + 8];
    int  escritos = snprintf(patron, sizeof(patron), "^%s$", expresion + 1);
    if (escritos <= 0 || (size_t)escritos >= sizeof(patron)) {
        liberar_indice(indice);
        return NULL;
    }

    regex_t regex;
    if (regcomp(&regex, patron, REG_EXTENDED) != 0) {
        liberar_indice(indice);
        return NULL;
    }

    size_t *coincidencias = (size_t *)malloc(sizeof(size_t) * indice->cantidad);
    if (coincidencias == NULL && indice->cantidad > 0) {
        regfree(&regex);
        liberar_indice(indice);
        return NULL;
    }

    size_t cantidad_coincidencias = 0;
    for (size_t i = 0; i < indice->cantidad; i++) {
        if (regexec(&regex, indice->entradas[i].ruta, 0, NULL, 0) == 0) {
            coincidencias[cantidad_coincidencias++] = i;
        }
    }
    regfree(&regex);

    if (cantidad_coincidencias == 0) {
        free(coincidencias);
        liberar_indice(indice);
        char *vacio = strdup("[]");
        return vacio;
    }

    FILE *fp_json = fopen(ruta_json, "rb");
    if (fp_json == NULL) {
        free(coincidencias);
        liberar_indice(indice);
        return NULL;
    }

    char *resultado = NULL;

    if (cantidad_coincidencias == 1) {
        EntradaIndice *entrada = &indice->entradas[coincidencias[0]];
        resultado = leer_fragmento_fp(fp_json, entrada->inicio, entrada->fin);
    } else {
        size_t tamano_total = 2;
        for (size_t i = 0; i < cantidad_coincidencias; i++) {
            EntradaIndice *entrada = &indice->entradas[coincidencias[i]];
            tamano_total += (size_t)(entrada->fin - entrada->inicio);
            if (i > 0) {
                tamano_total += 2;
            }
        }

        resultado = (char *)malloc(tamano_total + 1);
        if (resultado != NULL) {
            size_t pos = 0;
            resultado[pos++] = '[';
            for (size_t i = 0; i < cantidad_coincidencias; i++) {
                if (i > 0) {
                    resultado[pos++] = ',';
                    resultado[pos++] = ' ';
                }
                EntradaIndice *entrada = &indice->entradas[coincidencias[i]];
                long longitud_valor = entrada->fin - entrada->inicio;
                if (fseek(fp_json, entrada->inicio, SEEK_SET) != 0) {
                    free(resultado);
                    resultado = NULL;
                    break;
                }
                size_t leidos = fread(&resultado[pos], 1,
                                      (size_t)longitud_valor, fp_json);
                if ((long)leidos != longitud_valor) {
                    free(resultado);
                    resultado = NULL;
                    break;
                }
                pos += (size_t)longitud_valor;
            }
            if (resultado != NULL) {
                resultado[pos++] = ']';
                resultado[pos]   = '\0';
            }
        }
    }

    fclose(fp_json);
    free(coincidencias);
    liberar_indice(indice);
    return resultado;
}
