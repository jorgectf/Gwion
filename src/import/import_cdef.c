#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "traverse.h"
#include "instr.h"
#include "object.h"
#include "emit.h"
#include "gwion.h"
#include "operator.h"
#include "import.h"
#include "gwi.h"
#include "mpool.h"
#include "specialid.h"
#include "template.h"

ANN static m_bool mk_xtor(MemPool p, const Type type, const m_uint d, const enum tflag e) {
  VM_Code* code = e == tflag_ctor ? &type->nspc->pre_ctor : &type->nspc->dtor;
  const m_str name = type->name;
  *code = new_vmcode(p, NULL, SZ_INT, 1, name);
  (*code)->native_func = (m_uint)d;
  type->tflag |= e;
  return GW_OK;
}

ANN2(1,2) static inline m_bool class_parent(const Env env, Type t) {
  do {
    if(tflag(t, tflag_check))
      break;
    if(t->info->cdef)
      CHECK_BB(traverse_class_def(env, t->info->cdef))
  } while((t = t->info->parent));
  return GW_OK;
}

ANN2(1,2) static void import_class_ini(const Env env, const Type t) {
  t->nspc = new_nspc(env->gwion->mp, t->name);
  t->nspc->parent = env->curr;
  if(isa(t, env->gwion->type[et_object]) > 0)
    inherit(t);
  t->info->owner = env->curr;
  t->info->owner_class = env->class_def;
  env_push_type(env, t);
}

ANN2(1) void gwi_class_xtor(const Gwi gwi, const f_xtor ctor, const f_xtor dtor) {
  const Type t = gwi->gwion->env->class_def;
  if(ctor)
    mk_xtor(gwi->gwion->mp, t, (m_uint)ctor, tflag_ctor);
  if(dtor)
    mk_xtor(gwi->gwion->mp, t, (m_uint)dtor, tflag_dtor);
}

ANN static inline void gwi_type_flag(const Type t) {
  set_tflag(t, tflag_scan0 | tflag_scan1 | tflag_scan2 | tflag_check | tflag_emit);
}

ANN static Type type_finish(const Gwi gwi, const Type t) {
  gwi_add_type(gwi, t);
  import_class_ini(gwi->gwion->env, t);
  return t;
}

ANN2(1,2) Type handle_class(const Gwi gwi, Type_Decl *td) {
  DECL_OO(const Type, p, = known_type(gwi->gwion->env, td))
  CHECK_BO(class_parent(gwi->gwion->env, p))
  return p;
}

ANN2(1,2) Type gwi_class_ini(const Gwi gwi, const m_str name, const m_str parent) {
  struct ImportCK ck = { .name=name };
  CHECK_BO(check_typename_def(gwi, &ck))
  DECL_OO(Type_Decl *,td, = gwi_str2decl(gwi, parent ?: "Object"))
  Tmpl* tmpl = ck.tmpl ? new_tmpl_base(gwi->gwion->mp, ck.tmpl) : NULL;
  if(tmpl)
    CHECK_BO(template_push_types(gwi->gwion->env, tmpl))
  const Type base = find_type(gwi->gwion->env, td);
  const Type_List tl = td->types;
  if(tflag(base, tflag_ntmpl))
    td->types = NULL;
  const Type p = !td->types ? handle_class(gwi, td) : NULL;
  td->types = tl;
  if(tmpl)
    nspc_pop_type(gwi->gwion->mp, gwi->gwion->env->curr);
  CHECK_OO(p)
  const Type t = new_type(gwi->gwion->mp, ++gwi->gwion->env->scope->type_xid, s_name(ck.sym), p);
  t->info->cdef = new_class_def(gwi->gwion->mp, 0, ck.sym, td, NULL, loc(gwi));
  t->info->cdef->base.tmpl = tmpl;
  t->info->cdef->base.type = t;
  t->info->tuple = new_tupleform(gwi->gwion->mp, p);
  t->info->parent = p;
  if(td->array)
    set_tflag(t, tflag_typedef);
  if(ck.tmpl)
    set_tflag(t, tflag_tmpl);
  else
    gwi_type_flag(t);
  return type_finish(gwi, t);
}

ANN Type gwi_struct_ini(const Gwi gwi, const m_str name) {
  struct ImportCK ck = { .name=name };
  CHECK_BO(check_typename_def(gwi, &ck))
  const Type t = new_type(gwi->gwion->mp, ++gwi->gwion->env->scope->type_xid, s_name(ck.sym), gwi->gwion->type[et_compound]);
  set_tflag(t, tflag_struct);
  if(!ck.tmpl)
    gwi_type_flag(t);
  else {
    t->info->cdef = new_class_def(gwi->gwion->mp, 0, ck.sym, NULL, NULL, loc(gwi));
    t->info->cdef->base.type = t;
    t->info->cdef->base.tmpl = new_tmpl_base(gwi->gwion->mp, ck.tmpl);
    t->info->tuple = new_tupleform(gwi->gwion->mp, NULL);
    t->info->parent = NULL;
    t->info->cdef->cflag |= cflag_struct;
    set_tflag(t, tflag_tmpl);
  }
  return type_finish(gwi, t);
}

ANN m_int gwi_class_end(const Gwi gwi) {
  if(!gwi->gwion->env->class_def)
    GWI_ERR_B(_("import: too many class_end called."))
  nspc_allocdata(gwi->gwion->mp, gwi->gwion->env->class_def->nspc);
  env_pop(gwi->gwion->env, 0);
  return GW_OK;
}
