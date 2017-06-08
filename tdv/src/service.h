#ifndef SERVICE_H
#define SERVICE_H

#include <cppcms/application.h>  
#include <cppcms/applications_pool.h>  
#include <cppcms/service.h>
#include <cstdlib>
#include <algorithm>
#include "wiktdb.h"
#include "sparsearray.h"
#include "vectorize.h"
#include "stringutils.h"
#include "types.h"

class TDVService : public cppcms::application {
    WiktDB *wiktdb;
    
    public:
    TDVService(cppcms::service &);
    virtual string printRepr(SparseArray vec);
    virtual SparseArray getRepr();
    virtual void setHeaders();
    virtual void similar();
    virtual void definition();
    virtual void repr();
    virtual void similarity();
    virtual void features();
    virtual void wiktdef();
    virtual void disambiguation();
};

#endif
