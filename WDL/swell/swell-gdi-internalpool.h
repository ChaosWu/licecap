// used for HDC/HGDIOBJ pooling (to avoid excess heap use), used by swell-gdi.mm and swell-gdi-generic.cpp

#if defined(_DEBUG)
  #define SWELL_GDI_DEBUG
#endif

static WDL_Mutex *m_ctxpool_mutex;
#ifdef SWELL_GDI_DEBUG
  #include <assert.h>
  #include "../ptrlist.h"
  static WDL_PtrList<HDC__> *m_ctxpool_debug;
  static WDL_PtrList<HGDIOBJ__> *m_objpool_debug;
#else
  static HDC__ *m_ctxpool;
  static int m_ctxpool_size;
  static HGDIOBJ__ *m_objpool;
  static int m_objpool_size;
#endif



HDC__ *SWELL_GDP_CTX_NEW()
{
  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;
  
  HDC__ *p=NULL;
#ifdef SWELL_GDI_DEBUG
  m_ctxpool_mutex->Enter();
  if (!m_ctxpool_debug) m_ctxpool_debug = new WDL_PtrList<HDC__>;
  if (m_ctxpool_debug->GetSize() > 8192)
  {
    p =  m_ctxpool_debug->Get(0);
    m_ctxpool_debug->Delete(0);
    memset(p,0,sizeof(*p));
  }
  m_ctxpool_mutex->Leave();
#else
  if (m_ctxpool)
  {
    m_ctxpool_mutex->Enter();
    if ((p=m_ctxpool))
    { 
      m_ctxpool=p->_next;
      m_ctxpool_size--;
      memset(p,0,sizeof(*p));
    }
    m_ctxpool_mutex->Leave();
  }
#endif
  if (!p) 
  {
//    printf("alloc ctx\n");
    p=(HDC__ *)calloc(sizeof(HDC__)+128,1); // extra space in case things want to use it (i.e. swell-gdi-lice does)
  }
  return p;
}
static void SWELL_GDP_CTX_DELETE(HDC__ *p)
{
  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;

  if (!p) return;

  if (p->_infreelist) 
  {
#ifdef SWELL_GDI_DEBUG
    assert(!p->_infreelist);
#endif
    return;
  }

  memset(p,0,sizeof(*p));

#ifdef SWELL_GDI_DEBUG
  m_ctxpool_mutex->Enter();
  p->_infreelist=true;
  if (!m_ctxpool_debug) m_ctxpool_debug = new WDL_PtrList<HDC__>;
  m_ctxpool_debug->Add(p);
  m_ctxpool_mutex->Leave();
#else
  if (m_ctxpool_size<100)
  {
    m_ctxpool_mutex->Enter();
    p->_infreelist=true;
    p->_next = m_ctxpool;
    m_ctxpool = p;
    m_ctxpool_size++;
    m_ctxpool_mutex->Leave();
  }
  else 
  {
  //  printf("free ctx\n");
    free(p);
  }
#endif
}
static HGDIOBJ__ *GDP_OBJECT_NEW()
{
  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;
  HGDIOBJ__ *p=NULL;
#ifdef SWELL_GDI_DEBUG
  m_ctxpool_mutex->Enter();
  if (!m_objpool_debug) m_objpool_debug = new WDL_PtrList<HGDIOBJ__>;
  if (m_objpool_debug->GetSize()>8192)
  {
    p = m_objpool_debug->Get(0);
    m_objpool_debug->Delete(0);
    memset(p,0,sizeof(*p));
  }
  m_ctxpool_mutex->Leave();
#else
  if (m_objpool)
  {
    m_ctxpool_mutex->Enter();
    if ((p=m_objpool))
    {
      m_objpool = p->_next;
      m_objpool_size--;
      memset(p,0,sizeof(*p));
    }
    m_ctxpool_mutex->Leave();
  }
#endif
  if (!p) 
  {
    //   printf("alloc obj\n");
    p=(HGDIOBJ__ *)calloc(sizeof(HGDIOBJ__),1);    
  }
  return p;
}
static void GDP_OBJECT_DELETE(HGDIOBJ__ *p)
{
  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;
  if (!p) return;

  if (p->_infreelist) 
  {
#ifdef SWELL_GDI_DEBUG
    assert(!p->_infreelist);
#endif
    return;
  }

  memset(p,0,sizeof(*p));
#ifdef SWELL_GDI_DEBUG
  m_ctxpool_mutex->Enter();
  p->_infreelist = true;
  if (!m_objpool_debug) m_objpool_debug = new WDL_PtrList<HGDIOBJ__>;
  m_objpool_debug->Add(p);
  m_ctxpool_mutex->Leave();
#else
  if (m_objpool_size<200)
  {
    m_ctxpool_mutex->Enter();
    p->_infreelist = true;
    p->_next = m_objpool;
    m_objpool = p;
    m_objpool_size++;
    m_ctxpool_mutex->Leave();
  }
  else
  {
    //    printf("free obj\n");
    free(p);
  }
#endif
}

static bool HGDIOBJ_VALID(HGDIOBJ__ *p, int reqType=0)
{
  if (p == (HGDIOBJ__*)TYPE_PEN || p == (HGDIOBJ__*)TYPE_BRUSH ||
      p == (HGDIOBJ__*)TYPE_FONT || p == (HGDIOBJ__*)TYPE_BITMAP) return false;
#ifdef SWELL_GDI_DEBUG
  if (p) { assert(!p->_infreelist); }
#endif
  // insert breakpoints in these parts for debugging
  if (p && !p->_infreelist)
  {
#ifdef SWELL_GDI_DEBUG
    if (reqType) { assert(reqType == p->type); }
#endif

    return !reqType || reqType == p->type;
  }
  return false;
}

static bool HDC_VALID(HDC__ *ct)
{
#ifdef SWELL_GDI_DEBUG
  if (ct) { assert(!ct->_infreelist); }
#endif
  // insert breakpoints in these parts for debugging
  return ct && !ct->_infreelist;
}


static WDL_HeapBuf *m_tmpbuf_pool;

static WDL_HeapBuf *SWELL_GDP_GetTmpBuf()
{
  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;
  WDL_HeapBuf *ret=NULL;
  m_ctxpool_mutex->Enter();
  if (m_tmpbuf_pool)
  {
    ret = m_tmpbuf_pool;
    m_tmpbuf_pool = ret && ret->GetSize() == sizeof(WDL_HeapBuf *) ? *(WDL_HeapBuf **)ret->Get() : NULL;
  }
  m_ctxpool_mutex->Leave();
  if (!ret) ret = new WDL_HeapBuf;
  return ret;
}
static void SWELL_GDP_DisposeTmpBuf(WDL_HeapBuf *hb)
{
  if (!hb) return;

  if (!m_ctxpool_mutex) m_ctxpool_mutex=new WDL_Mutex;

  if (hb->ResizeOK(sizeof(void*),false))
  {
    m_ctxpool_mutex->Enter();
    *(WDL_HeapBuf **)hb->Get() = m_tmpbuf_pool;
    m_tmpbuf_pool = hb;
    m_ctxpool_mutex->Leave();
  }
  else delete hb;
}



#if !defined(SWELL_GDI_DEBUG) && defined(SWELL_CLEANUP_ON_UNLOAD)

class _swellGdiUnloader
{
  public:
  _swellGdiUnloader() { }
  ~_swellGdiUnloader() 
  {
     {
       HDC__ *p = m_ctxpool;
       m_ctxpool = NULL;
       while (p)
       {
         HDC__ *t = p;
         p = p->_next;
         free(t);
       }
     }
     {
       HGDIOBJ__ *p = m_objpool;
       m_objpool = NULL;
       while (p)
       {
         HGDIOBJ__ *t = p;
         p = p->_next;
         free(t);
       }
     }

     if (m_tmpbuf_pool)
     {
       WDL_HeapBuf *ret = m_tmpbuf_pool;
       m_tmpbuf_pool=0;

       while (ret)
       {
         WDL_HeapBuf *need_del = ret;
         ret = ret->GetSize()==sizeof(WDL_HeapBuf) ? *(WDL_HeapBuf**)ret->Get() : NULL;
         delete need_del;
       }
     }

     delete m_ctxpool_mutex;
     m_ctxpool_mutex=NULL;
  }
};

_swellGdiUnloader __swell__swellGdiUnloader;
#endif
