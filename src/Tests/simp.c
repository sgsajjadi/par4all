/* $RCSfile: simp.c,v $ (version $Revision$)
 * $Date: 1996/08/06 15:16:45 $, 
 */

/* test du simplex : ce test s'appelle par :
 *  programme fichier1.data fichier2.data ... fichiern.data
 * ou bien : programme<fichier.data
 * Si on compile grace a` "make sim" dans le directory
 *  /home/users/pips/C3/Linear/Development/polyedre.dir/test.dir
 * alors on peut tester l'execution dans le meme directory
 * en faisant : tests|more
 */

#include <stdio.h>
#include <malloc.h>
#include <setjmp.h>

extern jmp_buf overflow_error;

#include "boolean.h"
#include "assert.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

#define NB_INEQ sc->nb_ineq
#define NB_EQ sc->nb_eq
#define DIMENSION sc->dimension

static void
test_system(Psysteme sc)
{
    if (setjmp(overflow_error))
	fprintf(stdout, "*** Arithmetic error occured in simplex\n");
    else
	if (sc_simplexe_feasibility_ofl_ctrl(sc,FWD_OFL_CTRL))
	    printf("Systeme faisable (soluble) en rationnels\n") ;
	else
	    printf("Systeme insoluble\n");
}

static void 
test_file(FILE * f, char * name)
{
    Psysteme sc=sc_new(); 
    printf("systeme initial \n");
    if(sc_fscan(f,&sc)) 
    {
	printf("syntaxe correcte dans %s\n",name);
	sc_fprint(stdout, sc, *variable_default_name);
	printf("Nb_eq %d , Nb_ineq %d, dimension %d\n",
	       NB_EQ, NB_INEQ, DIMENSION) ;
	test_system(sc);
    }
    else
    {
	fprintf(stderr,"erreur syntaxe dans %s\n",name);
	exit(1);
    }
}

int 
main(int argc, char *argv[])
{
    /*  Programme de test de faisabilite'
     *  d'un ensemble d'equations et d'inequations.
     */
    FILE * f1;
    int i; /* compte les systemes, chacun dans un fichier */
    
    /* lecture et test de la faisabilite' de systemes sur fichiers */

    if(argc>=2) 
    {
	for(i=1;i<argc;i++)
	{
	    if((f1 = fopen(argv[i],"r")) == NULL) {
		fprintf(stdout,"Ouverture fichier %s impossible\n", argv[i]);
		exit(4);
	    }
	    test_file(f1, argv[i]);
	    fclose(f1) ;
	}
    }
    else 
    {
	test_file(stdin, "standard input");
    }
    exit(0) ;
}

