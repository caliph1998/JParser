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

static void array_push(JArray* arr, JsonValue* elem)
{
    if (arr->len == arr->cap)
    {
        arr->cap = 2 * arr->cap + 1;
        arr->items = xrealloc(arr->items, sizeof(JsonValue) * arr->cap);
    }
    arr->items[arr->len++] = elem;
}

static void object_put(JObject *obj, char *key, JsonValue *value)
{
    if(obj->len == obj->cap)
    {
        obj->cap = obj->cap * 2 + 1;
        obj->keys = xrealloc(obj->keys, sizeof(char*) * obj->cap);
        obj->values = xrealloc(obj->values, sizeof(JsonValue*) * obj->cap);
    }
    obj->keys[obj->len] = key;
    obj->values[obj->len++] = value;
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
            p->i++;
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
        if (match(p, ']'))
            break;
        if (!match(p, ','))
        {
            set_error(p, "expect , or ]");
            break;
        }
    }
    return arr;
}

static JsonValue *parse_object(Parser *p)
{
    skip_ws(p);
    if (!match(p, '{'))
    {
        set_error(p, "expected {");
        return NULL;
    }
    JsonValue *obj = JsonValue_new(JOBJECT);
    skip_ws(p);
    if (match(p, '}'))
        return obj;

    while (1)
    {
        JsonValue *k = parse_string(p);
        if (!k || k->type != JSTRING)
        {
            set_error(p, "object key must be string");
            return NULL;
        }
        if (!match(p, ':'))
        {
            set_error(p, "expected :");
            return NULL;
        }
        JsonValue *v = JsonValue_parse(p);
        if (!v)
            break;
        object_put(&obj->as.object, k->as.string, v);
        free(k); // key's string kept; JsonValue wrapper freed
        skip_ws(p);
        if (match(p,'}')) break;
        if (!match(p,',')) { set_error(p,"expected , or }"); break; }
    }
    return obj;
}

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
    return NULL;
}

typedef struct {
    const char *msg;
    size_t pos;
} JsonError;

JsonValue *json_parse(const char *text, JsonError *err)
{
    Parser p = {.s = text, .i = 0, .n=strlen(text), .err = NULL, .err_pos = 0};
    JsonValue *v = JsonValue_parse(&p);
    if (p.err)
    {
        if (err)
        {
            err->msg = p.err;
            err->pos = p.err_pos;
        }
        return NULL;
    }
    skip_ws(&p);
    if (p.i != p.n)
    {
        if (err)
        {
            err->msg = "trailing data";
            err->pos = p.i;
        }
        return NULL;
    }
    if (err)
    {
        err->msg = NULL;
        err->pos = 0;
    }
    return v;
}


int main()
{
    const char *txt = "{ \"name\":\"Ali\" }";
        // "{ \"name\":\"Ali\", \"ok\":true, \"n\":-12.3e+2,"
        // "  \"arr\":[1,2,3,\"x\"], \"obj\": {\"k\":\"v\"}, \"u\":\"\\u0627\" }";
    JsonError e;
    JsonValue *v = json_parse(txt, &e);
    if (!v)
    {
        fprintf(stderr, "Parse error at %zu: %s\n", e.pos, e.msg);
    }
    JsonValue_free(v);
}