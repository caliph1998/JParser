#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

typedef enum {
    JNULL, JBOOL, JNUMBER, JSTRING, JARRAY, JOBJECT
} JType;

typedef struct JsonValue JsonValue;

typedef struct {
    size_t len, cap;
    JsonValue **items;
} JArray;

typedef struct {
    size_t len, cap;
    char ** keys;
    JsonValue **values;
} JObject;

struct JsonValue {
    JType type;
    union {
        int boolean;
        double number;
        char *string;
        JArray array;
        JObject object;
    } as;
};

typedef struct {
    const char *s;
    size_t i, n;
    const char *err;
    size_t err_pos;
} Parser;

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q)
    {
        fprintf(stderr, "Out of memory\n"); 
        exit(1);
    }
    return q;
}

static void set_error(Parser *p, const char *msg) {
    if (!p->err)
    {
        p->err = msg;
        p->err_pos = p->i;
    }
}

static void skip_ws(Parser *p) {
    while (p->i < p->n && (
        p->s[p->i] == ' ' ||
        p->s[p->i] == '\t' ||
        p->s[p->i] == '\n' ||
        p->s[p->i] == '\r'
    ))
    {
        p->i++;
    }
}

static int match(Parser *p, char c) {
    skip_ws(p);
    if (p->i < p->n && p->s[p->i] == c)
    {
        p->i++;
        return 1;
    }
    return 0;
}

static JsonValue *JsonValue_new(JType t) {
    JsonValue *out = malloc(sizeof(JsonValue));
    if (!out)
    {
        fprintf(stderr, "OOM\n");
        exit(1);
    }
    out->type = t;
    if (t == JARRAY)
    {
        out->as.array.len = 0;
        out->as.array.cap = 0;
        out->as.array.items = NULL;
    }
    else if (t == JOBJECT)
    {
        out->as.object.len = 0;
        out->as.object.cap = 0;
        out->as.object.keys = NULL;
        out->as.object.values = NULL;
    }

    return out;
}

static void JsonValue_free(JsonValue* v) {
    if(v->type == JSTRING)
    {
        free(v->as.string);
    }
    else if (v->type == JARRAY)
    {
        for (int j = 0; j < v->as.array.len; j++)
        {
            JsonValue_free(v->as.array.items[j]);
            free(v->as.array.items);
        }
    }
    else if (v->type == JOBJECT)
    {
        for (int j = 0; j < v->as.object.len; j++)
        {
            JsonValue_free(v->as.object.values[j]);
            free(v->as.object.keys[j]);
        }
        free(v->as.object.keys);
        free(v->as.object.values);
    }

    free(v);
}


