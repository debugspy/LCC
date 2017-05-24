#include <stdlib.h>
#include <stdio.h>
#include "lcc.h"

Symbol *symtab = 0;
FILE *output = NULL;

Symbol *symbol_cast(void *ptr) {
    return (Symbol *) ptr;
}

Symbol *make_func_def_symbol(Type ret_type, String *name, Vector *param, Symbol *parent) {
    Symbol *ptr = symbol_cast(malloc(sizeof(Symbol)));
    ptr->self_type = DFUNC;
    ptr->ret_type = ret_type;
    ptr->name = name;
    ptr->param = param;
    ptr->parent = parent;
    ptr->assembly = make_assembly();
    ptr->stack_info.offset = ptr->stack_info.rsp = 0;
    return ptr;
}

Symbol *make_local_symbol(Type self_type, String *name, Symbol *parent) {
    Symbol *ptr = symbol_cast(malloc(sizeof(Symbol)));
    ptr->parent = parent;
    ptr->self_type = self_type;
    ptr->name = name;
    return ptr;
}

Symbol *make_new_scope(Symbol *parent) {
    Symbol *ptr = symbol_cast(malloc(sizeof(Symbol)));
    ptr->self_type = NEW_SCOPE;
    ptr->parent = parent;
    return ptr;
}

Symbol *make_param_symbol(Type type, String *name) {
    Symbol *ptr = symbol_cast(malloc(sizeof(Symbol)));
    ptr->self_type = type;
    ptr->name = name;
    return ptr;
}

Assembly *make_assembly() {
    Assembly *ptr = malloc(sizeof(Assembly));
    ptr->beg = make_list(NULL, NULL, NULL);
    ptr->end = make_list(ptr->beg, NULL, NULL);
    return ptr;
}

void assembly_push_back(Assembly *ptr, String *code) {
    make_list(ptr->end->prev, code, ptr->end);
}

void assembly_push_front(Assembly *ptr, String *code) {
    make_list(ptr->beg, code, ptr->beg->next);
}

void emit_func_signature(Assembly *code, String *s) {
    assembly_push_back(code, sprint("\t.globl %s\n\t.type  %s, @function", str(s), str(s)));
    assembly_push_back(code, sprint("%s:", str(s)));
}

void assembly_output(Assembly *ptr) {
    for (List_node *p = ptr->beg->next; p != ptr->end; p = p->next)
        fprintf(output, "%s\n", str(p->body));
}

void emit_func_arguments(Assembly *code, Analysis *func) {
    Assembly *al = make_assembly();
    for (int i = 0; i < size(func->param); i++) {
        Symbol *arg = symbol_cast(at(func->param, i));
        arg->stack_info.offset = allocate_stack(&func->stack_info, real_size[arg->self_type], al);
        assembly_push_back(al, sprint("\t# passing %s %d byte(s) %d(%%rbp)",
                                      str(arg->name), real_size[arg->self_type], -arg->stack_info.offset));
        assembly_push_back(al, sprint("\tmov%c   %s, %d(%%rbp)",
                                      op_suffix[arg->self_type],
                                      arugments_register[arg->self_type][i],
                                      -arg->stack_info.offset));
    }
    assembly_append(code, al);
}

Symbol *get_top_scope(Symbol *symtab) {
    Symbol *s = symtab;
    while (s != NULL && s->self_type != DFUNC) s = s->parent;
    return s;
}

int is_global_variable(Symbol *symtab) {
    Symbol *s = get_top_scope(symtab);
    return s == NULL;
}

void assembly_append(Assembly *p1, Assembly *p2) {
    if (p2 != NULL)
        append_list(p1->beg, p1->end, p2->beg, p2->end);
}

void emit_local_variable(Assembly *code, Symbol *s) {
    Symbol *func = get_top_scope(s);
    s->stack_info.offset = allocate_stack(&func->stack_info, real_size[s->self_type], code);
    assembly_push_back(code, sprint("\t# allocate %s %d byte(s) %d(%%rbp)",
                                    str(s->name), real_size[s->self_type], -s->stack_info.offset));
}

Symbol *make_func_decl_symbol(Type ret_type, String *name, Vector *param, Symbol *parent) {
    Symbol *decl = symbol_cast(malloc(sizeof(Symbol)));
    decl->self_type = DFUNC_NAME;
    decl->ret_type = ret_type;
    decl->name = name;
    decl->param = param;
    decl->parent = parent;
    return decl;
}

int allocate_stack(Stack *stack_info, int bytes, Assembly *code) {
    for (int i = 0; i < bytes; i++)
        if ((stack_info->offset + i + bytes) % bytes == 0) {
            int ori = stack_info->rsp;
            while (stack_info->offset + i + bytes > stack_info->rsp) stack_info->rsp += 16;
            if (stack_info->rsp > ori)
                assembly_push_back(code, sprint("\tsubq   $%d, %%rsp", stack_info->rsp - ori));
            return stack_info->offset += i + bytes;
        }
    return 0xFFFF;
}
