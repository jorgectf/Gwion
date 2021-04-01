#ifndef __ENVSET
#define __ENVSET

typedef m_bool (*envset_func)(const void*,const void*);
struct EnvSet {
  const Env env;
  const envset_func func;
  const void *data;
  const m_int scope;
  const enum tflag flag;
  m_bool run;
};

ANN m_bool envset_run(struct EnvSet*, const Type);
ANN2(1,3) m_bool envset_push(struct EnvSet*, const Type, const Nspc);
ANN2(1) void   envset_pop(struct EnvSet*, const Type);

ANN static inline m_bool extend_push(const Env env, const Type t) {
  if(t->info->parent)
    CHECK_BB(extend_push(env, t->info->parent))
  return env_push_type(env, t);
}

ANN static inline void extend_pop(const Env env, const Type t) {
  env_pop(env, 0);
  if(t->info->parent)
    extend_pop(env, t->info->parent);
}
#endif
