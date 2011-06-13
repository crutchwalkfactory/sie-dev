
/*
 * Этот файл является частью программы ElfLoader
 * Copyright (C) 2011 by Z.Vova, Ganster
 * Licence: GPLv3
 */

#include "loader.h"

const char libpath[] = "0:\\Misc\\elf3\\";

Global_Queue* lib_top;


__arch unsigned long elfhash(const char* name)
{
    unsigned long int hash = 0;
    while (*name != '\0')
    {
        unsigned long int hi;
        hash = (hash << 4) + *name++;
        hi = hash & 0xf0000000;
        if (hi != 0)
        {
            hash ^= hi >> 24;
            hash ^= hi;
        }
    }
    return hash;
}

__arch Elf32_Word findExport (Elf32_Exec* ex, const char* name)
{
    if(!ex || !ex->hashtab) return 0;

    long hash = elfhash(name);

    Elf32_Word nbucket = ex->hashtab[0];
    Elf32_Word nchain = ex->hashtab[1];
    Elf32_Word* bucket = &ex->hashtab[2];
    Elf32_Word* chain = &ex->hashtab[2 + nbucket];
    Elf32_Word func = 0;
    int symidx;

    for (symidx = bucket[hash % nbucket]; symidx != STN_UNDEF; symidx = chain[symidx])
    {
        Elf32_Sym sym = ex->symtab[symidx];

        /** îíî ïî÷åìó-òî íå äàâàëî íàéòè __cxa_begin_cleanup êîòîðûé ëèíêîâàëñÿ ê ñîøêå â âèäå
	      * ñòàòè÷íîé ëèáû
	      */
        //if (sym.st_value == 0 || sym.st_shndx == SHN_UNDEF)
        //    continue;

        switch (ELF_ST_TYPE(sym.st_info))
        {
        case STT_NOTYPE:
        case STT_FUNC:
        case STT_OBJECT:
            break;
        default:
            /* Not a code/data definition. */
            continue;
        }

        if (strcmp (ex->strtab + sym.st_name, name))
            /* Not the symbol we are looking for. */
            continue;

        switch (ELF_ST_BIND(sym.st_info))
        {
            case STB_LOCAL:
            case STB_GLOBAL:
            case STB_WEAK:
                /* Weak definition.  Use this value if we don't find another. */
                func = (Elf32_Word)ex->body + sym.st_value;
                break;
        default:
            /* Local symbols are ignored.  */
            break;
        }
    }

    return func;
}

__arch Elf32_Word dlsym(Elf32_Lib* lib, const char *name)
{
    if(!lib) return 0;
    return findExport(lib->ex, name);
}


// Çàãðóçêà áèáëèîòåêè
__arch Elf32_Lib* dlopen(const char *name)
{

    printf("Starting loading shared library...\n");
    int fp;
    Elf32_Ehdr ehdr;
    Elf32_Exec* ex;
    char fname[256];

    // Ñíà÷àëà èùåì â ãëîáàëüíîì ïóëëå
    Global_Queue* ready_libs = lib_top;
    while(ready_libs)
    {
        Elf32_Lib* lib = ready_libs->lib;

        if(!strcmp (lib->soname, name))
        {
            lib->users_cnt++;
            return lib;
        }
        ready_libs = ready_libs->prev;
    }

    // È åñëè óæ íå íàøëè - ïûòàåìñÿ ãðóçèòü
    sprintf(fname, "%s%s", libpath, name);

    /* îòêðûâàåì */
    if((fp = open(fname,A_ReadOnly+A_BIN,P_READ,&ferr)) == -1) return 0;

    /* ÷èòàåì */
    if(read(fp, &ehdr, sizeof(Elf32_Ehdr), &ferr) != sizeof(Elf32_Ehdr)) return 0;

    /* ïðîâåðÿåì */
    if(CheckElf(&ehdr)) return 0;

    /* àëëî÷èì */
    if( !(ex = malloc(sizeof(Elf32_Exec))) ) return 0;
    memcpy(&ex->ehdr, &ehdr, sizeof(Elf32_Ehdr));
    ex->v_addr = (unsigned int)-1;
    ex->fp = fp;
    ex->type = EXEC_LIB;
    ex->libs = 0;

    /* çàãðóæàåì ñåêöèè, âûïîëíÿåì ðåëîêàöèè, ïîäãðóæàåì ëèáû è ò.ï. */
    if( LoadSections(ex) ){
        close(fp, &ferr);
        elfclose(ex);
        return 0;
    }

    /* îí íàì óæå íå íóæåí */
    close(fp, &ferr);

    /* ñîçäàåì ëèáó */
    Elf32_Lib* lib;
    if( !(lib = malloc(sizeof(Elf32_Lib))) ){
        elfclose(ex);
        return 0;
    }

    lib->ex = ex;
    lib->users_cnt = 1;

#warning ïåðåäåëàòü íà ïóòü
    char* soname = ex->dyn[DT_SONAME] ? ex->strtab + ex->dyn[DT_SONAME] : (char*)name;
    strcpy(lib->soname, soname);

    /*  äëÿ ðåçèíîâîé áàçû)) */
    Global_Queue* global_ptr = malloc(sizeof(Global_Queue));
    if(!global_ptr)    // Ïî÷òè...íî íåò :'(
    {
        dlclose(lib);
        return 0;
    }

    /* èíèöèàëèçàöèÿ */
    global_ptr->lib = lib;
    global_ptr->next = 0;
    lib->glob_queue = global_ptr;

    if(lib_top)
    {
        lib_top->next = global_ptr;
        global_ptr->prev = lib_top;
    }
    else global_ptr->prev = 0;

    lib_top = global_ptr;

    run_INIT_Array(ex);

    /* îïöèîíàëüíûé âûçîâ ÷òî òî òèïà òî÷êè âõîäà â ëèáó */
    if(ex->dyn[DT_INIT])
    {
        printf("init function found\n");
        (( void (*)(const char*) )(ex->body + ex->dyn[DT_INIT] - ex->v_addr))(name);
    }

    printf(" Loade complete\n");
    return lib;
}



__arch int dlclose(Elf32_Lib* lib)
{
    if(!lib) return E_EMPTY;
    lib->users_cnt--;

    // Åñëè áîëüøå íå íóæíà - çàêðûâàåì
    if(!lib->users_cnt)
    {
        Elf32_Exec* ex = lib->ex;

        // Åñëè óæå èíèöèàëèçèðîâàíà ïîëíîñòüþ
        if(lib->glob_queue)
        {
            if(ex->dyn[DT_FINI]) ((LIB_FUNC*)(ex->body + ex->dyn[DT_FINI] - ex->v_addr))();

            Global_Queue* glob_queue = lib->glob_queue;

            Global_Queue* tmp = glob_queue->next;

            if(glob_queue == lib_top) lib_top = 0;
            if(tmp) tmp->prev = glob_queue->prev;
            if(tmp = glob_queue->prev) tmp->next = glob_queue->next;
            mfree(glob_queue);
        }

        elfclose(ex);
        mfree(lib);
    }
    return E_NO_ERROR;
}


