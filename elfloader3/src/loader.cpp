#include "loader.h"


static const unsigned char elf_magic_header[] =
  {0x7f, 0x45, 0x4c, 0x46,  /* 0x7f, 'E', 'L', 'F' */
   0x01,                    /* Only 32-bit objects. */
   0x01,                    /* Only LSB data. */
   0x01,                    /* Only ELF version 1. */
  };


/** ���� �� ���? � ��� ����� �� ���������? */
char isElfValid(Elf32_Ehdr *ehdr)
{
	if(memcmp(ehdr, elf_magic_header, sizeof(elf_magic_header)))
	{
		// �_�
		return 0;
	}
	return 1;
}



int elf_to_binary(char *data, int size)
{
	char *s = data;
	void *adr;
	Elf32_Ehdr ehdr;
	Elf32_Exec ex;

	ex.ehdr = &ehdr;
	ex.bin = data;
	ex.elfSize = size;
	ex.virtAdr = 0xFFFFFFFF;

	/** ����� ����� */
	memcpy(&ehdr, s, sizeof(Elf32_Ehdr));
	s += sizeof(Elf32_Ehdr);

	/** �������� */
	if( !isElfValid(&ehdr) )
	{
		printf("Bad elf\n");
		goto bad_rez;
	}

	dump(&ehdr);

	/** �����������*/
	adr = readSegments(&ex);


	//((int (*)())adr)();


	return 0;
	bad_rez:
	return 1;
}



void * readSegments(Elf32_Exec *ex)
{
	Elf32_Phdr *EPH = ( Elf32_Phdr *)(ex->bin + ex->ehdr->e_phoff);
    Elf32_Phdr *ESH = ( Elf32_Phdr *)(ex->bin + ex->ehdr->e_shoff);
    Elf32_Dyn* dyn_sect;

	printf("Elf32_Ehdr: %x\n", sizeof(Elf32_Ehdr));
	printf("Program header table is found at %X\n", EPH);
	printf("Sections header table is found at %X\n", ESH);

	printf("Number of sections: %i/%i\n",  ex->ehdr->e_phnum,  ex->ehdr->e_shnum);

	printf(" [+] Program header table struct dump\n");
	dump(EPH);
	printf(" [+] Sections header table struct dump\n");
	dump(ESH);

	ex->binarySize = calculateBinarySize(ex);
	printf("Raw binary output size: %d\n", ex->binarySize);

	if( !(ex->physAdr = (char*)malloc(ex->binarySize)) ){
		printf("Out of memory\n");
		return 0;
	}

	printf("physic addres: %08X\n", ex->physAdr);

	// ������
	int scnt = ex->ehdr->e_phnum;
	while (scnt--) {
 
		switch (EPH->p_type)
		{
            case PT_LOAD:
	 			if(EPH->p_filesz == 0) break; // Skip empty sections
				memcpy ((char*)VIRT2PHYS(ex->physAdr, ex->virtAdr, EPH->p_vaddr), (ex->bin + EPH->p_offset), EPH->p_filesz);
				printf("Loading CODE section (%X, size %i)\n", ex->bin + EPH->p_offset, EPH->p_filesz);
				//dump((char*)VIRT2PHYS(ex->physAdr, ex->virtAdr, EPH->p_vaddr), EPH->p_filesz);
				break;
            case PT_DYNAMIC:
				//dyn_sect = (Elf32_Dyn*)malloc(EPH->p_filesz);
				//memcpy (dyn_sect, ex->bin + EPH->p_offset, EPH->p_filesz);
                printf("Loading Dynamic section (%X, size %i)\n", ex->bin + EPH->p_offset, EPH->p_filesz);
                parseDynamicSection(ex, EPH);
                //free(dyn_sect);
                break;
            default:
                if(EPH->p_filesz != 0) printf("Unknown section (%i)\n", EPH->p_type);
        }
		EPH = ( Elf32_Phdr *)((unsigned char *)EPH + ex->ehdr->e_phentsize);
	}


	printf(" [+] EntryPoint: %X %X\n", VIRT2PHYS(ex->physAdr, ex->virtAdr, ex->ehdr->e_entry), ex->ehdr->e_entry-ex->virtAdr);
	return (void*)VIRT2PHYS(ex->physAdr, ex->virtAdr, ex->ehdr->e_entry);
}

int parseDynamicSection(Elf32_Exec *ex, Elf32_Phdr *EPH)
{
 	Elf32_Dyn *dyn_sect = (Elf32_Dyn*)(ex->bin + EPH->p_offset);
	Elf32_Word dyn[DT_BINDNOW+1];
 	int m = 0;
 	
 	printf("\n----- Parse Dynamic Section -----\n");
    while (dyn_sect[m].d_tag!=DT_NULL)
	{
		if (dyn_sect[m].d_tag<=DT_BINDNOW)
		{
  		   dyn[dyn_sect[m].d_tag]=dyn_sect[m].d_val;
		}
		m++;
    }
    for (m = 0; m <= DT_BINDNOW; m++)
	{
		printf("%i = %X\n", m, dyn[m]);
    }
    // Do Relocation
    if (dyn[DT_RELSZ]!=0)
	{
 	    m=0;
 	    long* addr;
 	    Elf32_Word r_type;
 	    Elf32_Rel* relTable = (Elf32_Rel*)(dyn_sect + dyn[DT_REL] - EPH->p_offset);
 	    while (m*sizeof(Elf32_Rel)<dyn[DT_RELSZ])
		{
  		 	r_type = ELF32_R_TYPE(relTable[m].r_info);
  		 	switch(r_type)
  		 	{
 			  	case R_ARM_NONE: break;
			 	case R_ARM_RABS32:
					 printf("R_ARM_RABS32: ");
					 addr = (long*)(ex->physAdr + ((Elf32_Rel*)(dyn_sect+dyn[DT_REL] - EPH->p_vaddr))[m].r_offset);
					 printf("from %X to %X\n", *addr, *addr + (long)(ex->physAdr - ex->virtAdr));
					 *addr+=(long)(ex->physAdr - ex->virtAdr);
					 break;
				case R_ARM_ABS32:
					 printf("R_ARM_ABS32: ");
					 addr = (long*)(ex->physAdr + ((Elf32_Rel *)(dyn_sect+dyn[DT_REL] - EPH->p_vaddr))[m].r_offset - ex->virtAdr);
					 printf("from %X to %X\n", *addr, *addr + (long)ex->physAdr);
					 *addr+=(long)ex->physAdr;
					 break;
				case R_ARM_RELATIVE:
					 printf("R_ARM_RELATIVE: ");
					 addr = (long*)(ex->physAdr + ((Elf32_Rel *)(dyn_sect+dyn[DT_REL] - EPH->p_vaddr))[m].r_offset-ex->virtAdr);
					 printf("from %X to %X\n", *addr, *addr + (long)(ex->physAdr - ex->virtAdr));
					 *addr+=(long)(ex->physAdr - ex->virtAdr);
					 break;
			 	default:
					 printf("Unknown REL type (%i)!\n", r_type);
			}
			m++;
 		}
	}
    printf("----- Parse Dynamic Section End -----\n\n");
 	return 0;
}


/** ������ ������� ��������� ����� */
int calculateBinarySize(Elf32_Exec *ex)
{
	unsigned int scnt = ex->ehdr->e_phnum, binsz = 0;
	unsigned long maxadr=0;
	unsigned int adr;
	Elf32_Phdr *ph = ( Elf32_Phdr *)(ex->bin + ex->ehdr->e_phoff);
	ex->virtAdr = 0xFFFFFFFF;

	while (scnt--)
	{
		if (ph->p_type == PT_LOAD)
		{
			if (ex->virtAdr > ph->p_vaddr)
					ex->virtAdr = ph->p_vaddr;

			adr = (ph->p_vaddr+ph->p_memsz);
		    if (maxadr < adr)
				maxadr = adr;
		}
		ph = ( Elf32_Phdr *)((unsigned char *)ph + ex->ehdr->e_phentsize);
	}

	return maxadr-ex->virtAdr;
}
