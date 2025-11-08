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

static int accept_keyword(Parser *p, const char *kw) 
{
    skip_ws(p);
    size_t k = 0, start = p->i;
    while(kw[k])
    {
        if (start + k >= p->n || p->s[start + k] != kw[k])
            return 0;
        k++;
    }
    p->i += k;
    return 1;
}

static JsonValue *parse_string(Parser *p)
{
    skip_ws(p);
    size_t start = p->i + 1;
    while(p->i < p->n)
    {
        p->i++;
        if (p->s[p->i] == '"')
        {
            size_t end = p->i;
            char* buf = malloc(end - start + 1);
            memcpy(buf, p->s + start, end - start);
            JsonValue *v = JsonValue_new(JSTRING);
            v->as.string = buf;
            return v;
        }
    }
    set_error(p, "unterminated string");
    return NULL;
}

static JsonValue *parse_number(Parser *p)
{
    skip_ws(p);
    size_t start = p->i;
    if (p->s[p->i] == '-') 
        p->i++;
    if (p->i < p->n && p->s[p->i]=='0') 
        p->i++;
    else 
    { 
        if (p->i==p->n || !isdigit((unsigned char)p->s[p->i])) 
            { 
                set_error(p,"bad number"); 
                return NULL;
            }
        while (p->i<p->n && isdigit((unsigned char)p->s[p->i])) 
            p->i++; 
    }
    if (p->i<p->n && p->s[p->i]=='.') 
    { 
        p->i++; 
        if (p->i==p->n || !isdigit((unsigned char)p->s[p->i])) 
            { 
                set_error(p,"bad frac"); 
                return NULL; 
            }
        while (p->i<p->n && isdigit((unsigned char)p->s[p->i])) 
            p->i++; 
    }
    if (p->i<p->n && (p->s[p->i]=='e'||p->s[p->i]=='E')) 
    {
        p->i++;
        if (p->i<p->n && (p->s[p->i]=='+'||p->s[p->i]=='-')) 
            p->i++;
        if (p->i==p->n || !isdigit((unsigned char)p->s[p->i])) 
        { 
            set_error(p,"bad exp"); 
            return NULL; 
        }
        while (p->i<p->n && isdigit((unsigned char)p->s[p->i])) 
            p->i++;
    }

    double val = strtod(p->s + start, NULL);
    JsonValue *v = JsonValue_new(JNUMBER);
    v->as.number = val;
    return v;
}

static JsonValue *JsonValue_parse(Parser *p); // forward dcl
static JsonValue *parse_array(Parser *p)
{
    if (!match(p,'['))
    {
        set_error(p, "expected [");
        return NULL;
    }
    JsonValue *arr = JsonValue_new(JARRAY);
    skip_ws(p);
    if (match(p, ']'))
        return arr;
    while (1)
    {
        JsonValue *elem = JsonValue_parse(p);
        if (!elem)
            return arr;
        array_push(&arr->as.array, elem);

    }
}

static JsonValue *parse_object(Parser *p);

static JsonValue *JsonValue_parse(Parser *p) {
    skip_ws(p);
    if (p->i >= p->n)
    {
        set_error(p, "unexpected end");
        return NULL;
    }

    char c = p->s[p->i];
    if (c == '"') 
        return parse_string(p);
    if (c == '-' || isdigit((unsigned char)c)) 
        return parse_number(p);
    if (c == '[') 
        return parse_array(p);
    if (c == '{') 
        return parse_object(p);
    if(accept_keyword(p, "true"))
    {
        JsonValue *v = JsonValue_new(JBOOL);
        v->as.boolean = 1;
        return v;
    }
    if (accept_keyword(p, "false"))
    {
        JsonValue *v = JsonValue_new(JBOOL);
        v->as.boolean = 0;
        return v;
    }
    if (accept_keyword(p, "null"))
    {
        JsonValue *v = JsonValue_new(JNULL);
        return v;
    }
}
