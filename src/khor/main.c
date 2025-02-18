#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LINE_IMPLEMENTATION
#include "line.h"
#define KHOR_IMPLEMENTATION
#include "khor.h"

void dump(khor_object const* self) {
    size_t k;
    switch (self->ty) {
        case KHOR_NUMBER:
            printf("%f", self->num.val);
            break;
        case KHOR_STRING: {
                char* last = self->str.ptr;
                printf("\"");
                for (k = 0; k < self->str.len; k++) {
                    char c;
                    switch (self->str.ptr[k]) {
                        case    7: c = 'a';  break;
                        case    8: c = 'b';  break;
                        case   27: c = 'e';  break;
                        case   12: c = 'f';  break;
                        case   10: c = 'n';  break;
                        case   13: c = 'r';  break;
                        case    9: c = 't';  break;
                        case   11: c = 'v';  break;
                        case  '"': c = '"';  break;
                        case '\\': c = '\\'; break;
                        default: continue;
                    }
                    printf("%.*s\\%c", (int)(self->str.ptr+k-last), last, c);
                    last = self->str.ptr+k+1;
                }
                printf("%.*s\"", (int)(self->str.ptr+k-last), last);
            }
            break;
        case KHOR_LIST:
            if (0 == self->lst.len) printf("()");
            else {
                printf("(list");
                for (k = 0; k < self->lst.len; k++) {
                    printf(" ");
                    dump(self->lst.ptr+k);
                }
                printf(")");
            }
            break;
        case KHOR_LAMBDA:
            printf("(lambda (..%u) <..>)", self->lbd.ary);
            break;
        default:
            printf("%s", self->sym.txt);
            break;
    }
}

void dumpcode(khor_bytecode const* code) {
    unsigned cp, k;
    for (cp = 0; cp < code->len; cp++) {
        printf("[+%4u] ", cp);
        switch (code->ptr[cp]) {
            case KHOR_OP_MKNUM:
                printf("mknum    %f", *(double*)(code->ptr+cp+1));
                cp+= 8;
                break;
            case KHOR_OP_MKSTR:
                k = code->ptr[cp+1]<<16 | code->ptr[cp+2]<<8 | code->ptr[cp+3];
                printf("mkstr    %u", k);
                cp+= 3+k;
                break;
            case KHOR_OP_MKLST:
                k = code->ptr[cp+1]<<16 | code->ptr[cp+2]<<8 | code->ptr[cp+3];
                printf("mklst    %u", k);
                cp+= 3;
                break;
            /*case KHOR_OP_MKLBD:   printf("mklbd");   break;*/
            case KHOR_OP_MKSYM:
                printf("mksym    ");
                for (k = 0; KHOR_SYMBOL < code->ptr[cp+1]; k++, cp++) printf("%c", code->ptr[cp+1]);
                break;
            case KHOR_OP_MKNIL:   printf("mknil");   break;
            case KHOR_OP_HALT:    printf("halt     %d", code->ptr[++cp]); break;
            case KHOR_OP_DISCARD: printf("discard"); break;
            case KHOR_OP_RESOLVE: printf("resolve"); break;
            case KHOR_OP_DEFINE:  printf("define");  break;
            case KHOR_OP_APPLY:   printf("apply    %d", code->ptr[++cp]); break;
            case KHOR_OP_JUMP:
                k = code->ptr[cp+1]<<8 | code->ptr[cp+2];
                printf("jump     %d (-> %u)", k, cp+3+k);
                cp+= 2;
                break;
            case KHOR_OP_JUMPIF:
                k = code->ptr[cp+1]<<8 | code->ptr[cp+2];
                printf("jumpif   %d (-> %u)", k, cp+3+k);
                cp+= 2;
                break;
            case KHOR_OP_ADD:     printf("add");     break;
            case KHOR_OP_SUB:     printf("sub");     break;
            case KHOR_OP_MUL:     printf("mul");     break;
            case KHOR_OP_DIV:     printf("div");     break;
            case KHOR_OP_LT:      printf("lt");      break;
            case KHOR_OP_GT:      printf("gt");      break;
            case KHOR_OP_LE:      printf("le");      break;
            case KHOR_OP_GE:      printf("ge");      break;
            case KHOR_OP_EQ:      printf("eq");      break;
            case KHOR_OP_AND:     printf("and");     break;
            case KHOR_OP_OR:      printf("or");      break;
            case KHOR_OP_CATS:    printf("cats");    break;
            default:
                printf(' ' <= code->ptr[cp] && code->ptr[cp] <= '~' ? "; err: 0x%02X (%c)\n" : "; err: 0x%02X", (unsigned)code->ptr[cp], code->ptr[cp]);
        }
        printf("\n");
    }
#   if 24 != KHOR_COUNT_OPS
#   error "missing op in dumpcode!"
#   endif
}

void dumpenv(khor_environment const* env) {
    size_t k;
    for (k = 0; k < env->entries.len; k++) {
        khor_enventry const* it = env->entries.ptr+k;
        static char const* tynames[] = {"Number", "String", "List", "Lambda", "Symbol"};
        printf("%s :: %s\n", it->key.txt, tynames[it->value.ty < 4 ? it->value.ty : 4]);
    }
}

void dumpmacros(khor_ruleset const* macros) {
    size_t i, j, k;
    for (k = 0; k < macros->len; k++) {
        khor_rules const* it = macros->ptr+k;
        printf("%s :=\n", it->key.txt);
        for (j = 0; j < it->rules.len; j++) {
            printf("\t");
            for (i = 0; i < it->rules.ptr[j].names.len; i++)
                printf(" %s", it->rules.ptr[j].names.ptr[i].txt);
            printf(" -> ");
            dump(&it->rules.ptr[j].subst);
            puts("");
        }
    }
}

bool thandle(khor_bytecode* code, khor_environment* env, khor_stack* stack, unsigned* cp, unsigned* sp, char w) {
    bool so;
    (void)env;
    printf("- interrupted (%d) -\n", w);
    printf("next [%u] 0x%02X\n", *cp+1, code->ptr[*cp+1]);
    printf("top %u/%lu ", *sp, stack->len);
    if (*sp) dump(stack->ptr+*sp-1);
    so = stack->len == *sp;
    if (so) puts("stack overflow!");
    getchar();
    puts("- end -");
    return !so;
}

int main(int argc, char** argv) {
    size_t k;
    khor_slice source;
    khor_object ast;
    khor_ruleset macros = {0};
    khor_bytecode code = {0};
    khor_environment env = {0};
    khor_stack stack = {0};

    (void)dyarr_insert(&stack, 0, 1024);

    for (k = 1; (int)k < argc; k++) if ('-' == argv[k][0]) switch (argv[k][1]) {
        case 'h':
            printf("Usage: %s <idk>\n", argv[0]);
            return 1;

        case 'p':
            if ((int)++k == argc) {
                printf("Expected argument for -p\n");
                return 1;
            } else {
                FILE* f = fopen(argv[k], "r");
                if (!f) {
                    printf("Could not read file '%s'\n", argv[k]);
                    return 1;
                }
                fseek(f, 0, SEEK_END);
                source.len = ftell(f);
                source.ptr = malloc(source.len);
                if (source.ptr) {
                    char* txt = source.ptr;
                    fseek(f, 0, SEEK_SET);
                    fread(source.ptr, source.len, 1, f);

                    while (source.len) {
                        ast = khor_parse(&source, &macros);
                        khor_compile(&ast, &code);
                        khor_destroy(&ast);
                        khor_eval(&code, &env, &stack, thandle);
                        dyarr_clear(&code);
                        khor_destroy(stack.ptr);
                    }

                    free(txt);
                }
                fclose(f);
            }
            break;

        case 'c':
            if ((int)++k == argc) {
                printf("Expected argument for -c\n");
                return 1;
            }
            source.len = strlen(argv[k]);
            source.ptr = argv[k];

            ast = khor_parse(&source, &macros);
            khor_compile(&ast, &code);
            khor_destroy(&ast);
            khor_eval(&code, &env, &stack, thandle);
            dyarr_clear(&code);
            khor_destroy(stack.ptr);

            break;

    } else printf("Unexpected argument: '%s'\n", argv[k]);

    while ((source.ptr = line_read())) if (*source.ptr) {
#       define cmdeq(__c) (!memcmp(__c, source.ptr, strlen(__c)))
        bool showast = false, showbc = false, norun = false;

        if (cmdeq(".help")) {
            puts("list of command:\n\t.help\n\t.env\n\t.macros\n\t.quit\n\t.ast <source>\n\t.bc <source>\n\t. <source>");
            continue;
        } else if (cmdeq(".env")) {
            puts("{{{ env:");
            dumpenv(&env);
            puts("}}}");
            continue;
        } else if (cmdeq(".macros")) {
            puts("{{{ macros:");
            dumpmacros(&macros);
            puts("}}}");
            continue;
        } else if (cmdeq(".quit")) break;

        if ((showast = cmdeq(".ast"))) source.ptr+= 4;
        if ((showbc = cmdeq(".bc"))) source.ptr+= 3;
        if ((norun = cmdeq(". "))) source.ptr++;

        if (cmdeq(".")) {
            puts("unknown command");
            continue;
        }
#       undef cmdeq

        source.len = strlen(source.ptr);
        ast = khor_parse(&source, &macros);

        if (showast) {
            puts("{{{ ast:");
            dump(&ast);
            puts("\n}}}");
        }

        khor_compile(&ast, &code);
        khor_destroy(&ast);

        if (showbc) {
            puts("{{{ bc:");
            dumpcode(&code);
            puts("}}}");
        }

        if (norun) {
            dyarr_clear(&code);
            continue;
        }

        khor_eval(&code, &env, &stack, thandle);
        dyarr_clear(&code);

        dump(stack.ptr);
        puts("");
        khor_destroy(stack.ptr);
    }
    line_free();

    for (k = 0; k < macros.len; k++) {
        size_t j;
        for (j = 0; j < macros.ptr[k].rules.len; j++) {
            khor_destroy(&macros.ptr[k].rules.ptr[j].subst);
            free(macros.ptr[k].rules.ptr[j].names.ptr);
        }
        free(macros.ptr[k].rules.ptr);
    }
    for (k = 0; k < env.entries.len; k++)
        khor_destroy(&env.entries.ptr[k].value);

    return 0;
}
