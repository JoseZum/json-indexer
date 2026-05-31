#ifndef LIBJSONINDEX_H
#define LIBJSONINDEX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LONGITUD_MAX_RUTA 4096

typedef struct {
    char *ruta;
    long  inicio;
    long  fin;
} EntradaIndice;

typedef struct {
    EntradaIndice *entradas;
    size_t         cantidad;
    size_t         capacidad;
} Indice;

typedef enum {
    RESULTADO_EXITO              =  0,
    ERROR_ARCHIVO_NO_ENCONTRADO  = -1,
    ERROR_MEMORIA                = -2,
    ERROR_JSON_INVALIDO          = -3,
    ERROR_REGEX_INVALIDO         = -4,
    ERROR_EXPRESION_INVALIDA     = -5,
    ERROR_ESCRITURA              = -6
} CodigoResultado;

int generar_indice(const char *ruta_json, const char *ruta_indice);

Indice *cargar_indice(const char *ruta_indice);

void liberar_indice(Indice *indice);

char *extraer_fragmento(const char *ruta_json, long inicio, long fin);

char *buscar_en_indice(const char *ruta_json,
                       const char *ruta_indice,
                       const char *expresion);

#ifdef __cplusplus
}
#endif

#endif
