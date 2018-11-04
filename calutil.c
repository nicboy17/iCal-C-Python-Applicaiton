/***************************************************************************
 calutil.c -- iCalendar utility functions. Library that reads in .ics files.

 Created: Jan.13 2016
 Author: Nick Major #0879292
 Contact: nmajor@mail.uoguelph.ca
***************************************************************************/

#include "calutil.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <assert.h>


CalError checkVerId( CalComp *comp );
int checkCompName( CalComp *comp );

char *parseCalName ( char *name, char *values );
CalParam *parseCalParam( char *buff, int *paramNum );
CalParam *addToCalParam( CalParam *head, char *name, char *values );
CalProp *addToCalProp( CalProp *head, CalProp *add, int size );

char *parseCalUpperCase( char *str );
bool parseCalQuotes( char *str );
void freeCalProp( CalProp *prop );

void printCalComp( CalComp * comp );

CalStatus writeCalLine( FILE *ics, char *str );
void unParseCalProp( CalProp *prop, char *str );
void printCalComp( CalComp * comp );


CalStatus writeCalComp( FILE *const ics, const CalComp *comp )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = 0;
    error.lineto = 0;

    char buff[5000] = "";//temporary string
    CalProp *temp;//temporary prop struct
    static int depth = 0;// depth counter
    static char names[75] = "";

    if( ics == NULL )
    {
      depth = 0;
      strcpy( names, "");
      return error;
    }

    //Start of component
    sprintf( buff, "BEGIN:%s", comp->name );
    if( depth == 0 )
    {
        strcpy( names, comp->name );
    }
    error = writeCalLine( ics, buff );
    depth ++;

    //writes prop to file
    temp = comp->prop;
    for( int i = 0; i < comp->nprops && temp != NULL; i ++ )
    {
        unParseCalProp( temp, buff );
        error = writeCalLine( ics, buff );
        temp = temp->next;
    }

    //recursivly calls writeCalComp
    for( int i = 0; i < comp->ncomps; i ++ )
    {
        error = writeCalComp( ics, comp->comp[i] );
        sprintf( buff, "END:%s", comp->comp[i]->name );
        error = writeCalLine( ics, buff );
        depth --;
    }

    //ends writing
    if( depth == 1 || depth == 0)
    {
        sprintf( buff, "END:%s", names );
        error = writeCalLine( ics, buff );
    }

    return error;
}


/* New */
/*
 Adds SYNTAX and properties into string
 Arguments: property, string to be returned containing new property and syntax
 Return value: string of prop line
 */
void unParseCalProp( CalProp *prop, char *str )
{
    //simple unparse
    if( prop->nparams == 0 )
    {
        sprintf( str, "%s:%s", prop->name, prop->value );
    }

    //complex unparse
    else
    {
        //prop name
        sprintf( str, "%s;", prop->name );
        for( int j = 0; j < prop->nparams; j ++ )
        {
            //param name
            sprintf( str, "%s%s=", str, prop->param->name );
            if( prop->param->nvalues > 1 )
            {
                //param values
                for( int i = 0; i < prop->param->nvalues; i ++ )
                {
                    if( i == prop->param->nvalues && j < prop->nparams )
                    {
                        sprintf( str, "%s%s;", str, prop->param->value[i] );
                    }
                    else if(  i < prop->param->nvalues-1 )
                    {
                        sprintf( str, "%s%s,", str, prop->param->value[i] );
                    }
		    else
		      {
			strcat( str, prop->param->value[i] );
		      }
                }
            }
            else
            {
                strcat( str, prop->param->value[0] );
            }

	   if( j != prop->nparams-1 )
            {
                strcat( str, ";" );
            }

            prop->param = prop->param->next;
        }

        //prop value
        sprintf( str, "%s:%s", str, prop->value );
    }
}


/* New */
/*
 writes string to file and folds if greater than 75 character
 Arguments: File to write, string to write to file
 Return value: error struct
 */
CalStatus writeCalLine( FILE *ics, char *str )
{
    static int lines = 0;//track of total lines written

    CalStatus error;
    error.code = OK;
    error.linefrom = 0;
    error.lineto = 0;

    int charCount = 0;
    int check = 0;//checks write to file

    //simple no fold
    if( strlen( str ) <= FOLD_LEN )
    {
        strcat( str, "\r\n" );
        check = fputs( str, ics );
        if( check < 0 )
        {
            error.code = IOERR;
            return error;
        }
        lines ++;
        error.linefrom = lines;
    }

    //folds after 75 characters
    else
    {
        error.linefrom = lines + 1;
        for( int i = 0; i < strlen(str); i ++ )
        {
            //prints character at a time
            check = fputc( str[i], ics );
            charCount ++;

            if( check < 0 )
            {
                error.code = IOERR;
                return error;
            }

	    //after 75 characters
            if( i == strlen(str)-1 )
            {
                fputs( "\r\n", ics );
                if( check < 0 )
                {
                    error.code = IOERR;
                    return error;
                }

                lines ++;
            }

            else if( charCount == FOLD_LEN )
            {
                fputs( "\r\n ", ics );
                if( check < 0 )
                {
                    error.code = IOERR;
                    return error;
                }

                charCount = 1;
                lines ++;
            }
	}
    }

    error.lineto = lines;
    return error;
}


CalStatus readCalFile( FILE *const ics, CalComp **const pcomp )
{
    CalStatus errors;
    errors.code = OK;
    char *temp = NULL;

    //Initialize readCalLine
    readCalLine( NULL, NULL );

    //Allocate memory
    *pcomp = malloc( sizeof(CalComp) );
    assert( *pcomp != NULL );
    (*pcomp)->name = NULL;
    (*pcomp)->nprops = 0;
    (*pcomp)->prop = NULL;
    (*pcomp)->ncomps = 0;

    errors = readCalComp( ics, pcomp );

    //Frees all memory if there is any error
    if ( errors.code != OK )
    {
        freeCalComp(*pcomp);
        return errors;
    }
    //checks number of sub components
    else if( (*pcomp)->ncomps == 0 )
    {
        errors.code = NOCAL;
        return errors;
    }

    //checks version and prodid
    errors.code = checkVerId( *pcomp );
    if ( errors.code != OK )
    {
        freeCalComp(*pcomp);
        return errors;
    }

    //makes sure there is at least 1 V component
    if ( checkCompName( *pcomp ) < 1 )
    {
        errors.code = NOCAL;
        freeCalComp(*pcomp);
        return errors;
    }

    //checks to see if there is more to read in
    errors = readCalLine( ics, &temp );
    if ( temp != NULL )
    {
        errors.code = AFTEND;
        freeCalComp(*pcomp);
        return errors;
    }

    //frees temp if it allocated
    if( temp != NULL )
    {
        free( temp );
    }

    return errors;
}


CalStatus readCalComp( FILE *const ics, CalComp **const comp )
{
    CalStatus errors;
    errors.code = OK;
    static int depth = 0;//counts depth of components
    char *pbuff = NULL;//temp string holder

    CalProp *prop = NULL;

    if( (*comp)->name == NULL && (*comp)->ncomps == 0 )
    {
        errors = readCalLine( ics, &pbuff );
        if( pbuff == NULL )
        {
            errors.code = NOCAL;
            return errors;
        }
        prop = malloc( sizeof(CalProp) );
        assert( prop != NULL );
        prop->nparams = 0;
        prop->param = NULL;
        prop->next = NULL;
        errors.code = parseCalProp( pbuff, prop );
        free( pbuff );

        //very first line
        if( strcasecmp( prop->name, "BEGIN" ) == 0 && strcasecmp( prop->value, "VCALENDAR" ) == 0 )
        {
            *comp = malloc( sizeof(CalComp) );
            assert( comp != NULL );
            (*comp)->name = malloc( sizeof(char)*(strlen(prop->value)+1) );
            assert( (*comp)->name != NULL );

            strcpy( (*comp)->name, prop->value );
            (*comp)->prop = NULL;
            (*comp)->nprops = 0;
            (*comp)->ncomps = 0;
            depth = 1;
            freeCalProp( prop );
        }
        //NOCAL if first line doesn't contain begin and/or vcalendar
        else if( strcasecmp( prop->name, "BEGIN" ) != 0 || strcasecmp( prop->value, "VCALENDAR" ) != 0 )
        {
            errors.code = NOCAL;
            freeCalProp( prop );
            return errors;
        }
    }

    if( (*comp)->name != NULL )
    {
        errors = readCalLine( ics, &pbuff );
        if( pbuff != NULL )
        {
            prop = malloc( sizeof(CalProp) );
            assert( prop != NULL );
            prop->nparams = 0;
            prop->param = NULL;
            prop->next = NULL;
            errors.code = parseCalProp( pbuff, prop );
        }
        else
        {
            free( prop );
            return errors;
        }
        free( pbuff );

        //reads until final line
        while( !(strcasecmp( prop->name, "END" ) == 0 && strcasecmp( prop->value, "VCALENDAR" ) == 0) )
        {
            //adds properties
            if( strcasecmp( prop->name, "BEGIN" ) != 0 && strcasecmp( prop->name, "END" ) != 0 )
            {
                (*comp)->prop = addToCalProp( (*comp)->prop, prop, (*comp)->nprops );
                (*comp)->nprops ++;

                freeCalProp( prop );
            }
            //proper end of subcomponent results in lower depth
            else if( strcasecmp( prop->name, "END" ) == 0 && strcasecmp( prop->value, (*comp)->name ) == 0 )
            {
                depth --;
                freeCalProp( prop );
                return errors;
            }
            //component flexible array
            else if( strcmp( prop->name, "BEGIN" ) == 0 )
            {
                *comp = realloc(*comp, sizeof(CalComp) + sizeof(CalComp*)*((*comp)->ncomps+1) );
                assert( comp != NULL );
                (*comp)->comp[(*comp)->ncomps] = malloc( sizeof(CalComp) );
                assert( (*comp)->comp[(*comp)->ncomps] != NULL );
                (*comp)->comp[(*comp)->ncomps]->ncomps = 0;
                (*comp)->comp[(*comp)->ncomps]->prop = NULL;
                (*comp)->comp[(*comp)->ncomps]->nprops = 0;
                (*comp)->comp[(*comp)->ncomps]->name = malloc( sizeof(char)*(strlen(prop->value)+1) );
                assert( (*comp)->comp[(*comp)->ncomps]->name  != NULL );
                strcpy( (*comp)->comp[(*comp)->ncomps]->name, prop->value );

                depth ++;
                // depth is greater than 4
                if( depth == 4 )
                {
                    errors.code = SUBCOM;
                    return errors;
                }
                else
                {
                    //calls readCalComp recursively
                    errors = readCalComp( ics, &(*comp)->comp[(*comp)->ncomps] );
                }

                (*comp)->ncomps ++;
                freeCalProp( prop );
            }

            //if everything is OK, countinues reading and parsing
            if( depth < 4 && errors.code == OK)
            {
                errors = readCalLine( ics, &pbuff );
                if( pbuff != NULL )
                {
                    prop = malloc( sizeof(CalProp) );
                    assert( prop != NULL );
                    prop->nparams = 0;
                    prop->param = NULL;
                    prop->next = NULL;
                    errors.code = parseCalProp( pbuff, prop );
                    free( pbuff );
                }
                else
                {
                    errors.code = BEGEND;
                    return errors;
                }
            }
            else
            {
                return errors;
            }
        }
    }

    //checks for NODATA
    if( (*comp)->ncomps == 0 && (*comp)->nprops == 0 )
    {
        errors.code = NODATA;
        if( prop != NULL )
        {
            freeCalProp( prop );
        }
        return errors;
    }

    //frees if prop is allocated
    if( prop != NULL )
    {
        freeCalProp( prop );
    }

    return errors;
}


/*
 adds (deep copy) property to component property list
 Arguments: head of component prop list, property to add, size of comp prop
 Return value: head of component property list
 */
CalProp *addToCalProp( CalProp *head, CalProp *add, int size )
{
    //initial head
    if( size == 0 )
    {
        head = malloc( sizeof(CalProp) );
        assert( head != NULL );

        head->name = malloc( sizeof(char)*(strlen(add->name)+1) );
        assert( head->name != NULL );
        strcpy( head->name, add->name );

        head->value = malloc( sizeof(char)*(strlen(add->value)+1) );
        assert( head->value != NULL );
        strcpy( head->value, add->value );

        head->nparams = add->nparams;
        head->param = add->param;
        head->next = NULL;
    }
    else
    {
        // adds to end
        CalProp *temp = head;
        while( temp->next != NULL )
        {
            temp = temp->next;
        }
        temp->next = malloc( sizeof(CalProp) );
        assert( temp->next != NULL );

        temp->next->name = malloc( sizeof(char)*(strlen(add->name)+1) );
        assert( temp->next->name != NULL );
        strcpy( temp->next->name, add->name );

        temp->next->value = malloc( sizeof(char)*(strlen(add->value)+1) );
        assert( temp->next->value != NULL );
        strcpy( temp->next->value, add->value );

        temp->next->nparams = add->nparams;
        temp->next->param = add->param;
        temp->next->next = NULL;
    }

    return head;
}


CalStatus readCalLine( FILE *const ics, char **const pbuff )
{
    static int lines = 0;//local variable for total lines read
    static int skip = 0;//local variable for just \r\n blank lines
    static char aheadLine[5000];

    CalStatus errors;
    errors.code = OK;

    //boolean will be true if the line is blank with onlr \r\n
    bool previous = false;

    if( ics == NULL )
    {
        //resets static
        errors.code = OK;
        skip = 0;
        errors.linefrom = 0;
        errors.lineto = 0;
        strcpy( aheadLine, "" );
        lines = 0;

    }
    else
    {
        //temp string holders
        char temp[5000] = "";
        char *token = NULL;
        char *check = NULL;
        bool fold = false;
        lines ++;
        errors.linefrom = lines + skip;

        if( strcmp(aheadLine, "" ) != 0 )
        {
            if( aheadLine[0] == '|' )
            {
                *pbuff =  NULL;
                errors.linefrom --;
                errors.lineto = errors.linefrom;
                return errors;
            }
            *pbuff = malloc( sizeof(char)*(strlen(aheadLine)+1) );
            assert( pbuff != NULL );
            assert( *pbuff != NULL );
            strcpy( *pbuff, aheadLine );
            strcpy( aheadLine, "" );
        }
        else
        {
            check = fgets( temp, 5000, ics );
            if( check == NULL )
            {
                *pbuff = NULL;
                errors.linefrom --;
                errors.lineto = errors.linefrom;
                return errors;
            }
            else
            {
                *pbuff = malloc( sizeof(char)*(strlen(temp)+1) );
                assert( *pbuff != NULL );
                strcpy( *pbuff, temp );
            }
        }

        //reads ahead
        check = fgets( temp, 5000, ics );
        if( check != NULL )
        {
            //end of file
            if( temp[strlen(temp)-2] != '\r' && temp[strlen(temp)-2] != '\n' )
            {
                strcpy(aheadLine, "|" );
            }
            //skips ahead of blank lines
            while( strcmp(temp, "\r\n") == 0 )
            {
                fgets( temp, 5000, ics );
                skip ++;
                previous = true;
            }
            while( strcmp(temp, "\t\r\n") == 0 || strcmp(temp, " \r\n") == 0 )
            {
                fgets( temp, 5000, ics );
                lines ++;
            }
            //makes sure string is valid
            if( temp[0] == ' ' || temp[0] == '\t' )
            {
                temp[0] = '|'; // adds parsing value
                fold = true;
                while ( fold == true )
                {
                    *pbuff = strtok(*pbuff, "\r");
                    *pbuff = strtok(*pbuff, "\n");
                    token = strtok( temp, "|" );
                    //appends folded line
                    *pbuff = realloc(*pbuff, strlen(*pbuff) + strlen(token)+1 );
                    assert( *pbuff != NULL );
                    strcat( *pbuff, token );
                    strcpy( temp, "" );
                    fgets( temp, 4999, ics );

                    //makes sure string is valid
                    if( (temp[0] == ' ' || temp[0] == '\t') )
                    {
                        temp[0] = '|';
                        fold = true;
                    }
                    else
                    {
                        fold = false;
                    }
                    lines++;
                }
                strcpy(aheadLine, temp );
            }
            else
            {
                strcpy(aheadLine, temp );
            }
        }
        else if( check == NULL )
        {
            strcpy(aheadLine, "|" );
        }


        //removes any left over newline characters
        if( *pbuff != NULL )
        {
            *pbuff = strtok(*pbuff, "\r");
            *pbuff = strtok(*pbuff, "\n");
        }
        //printf( "%s\n", *pbuff );
    }

    if( previous == true )
    {
        errors.lineto = lines;
    }
    else
    {
        errors.lineto = lines + skip;
    }
    return errors;
}


CalError parseCalProp( char *const buff, CalProp *const prop )
{
    CalError error = OK;
    bool semi = false;//boolean parameters
    bool quotes = false;//true if come across quote, false when comes across other
    bool colon = false;//checks for first colon
    char *token;
    char * paramText = NULL;
    char temp[strlen(buff)+1];//copied over to static because of bus error
    strcpy(temp, buff);

    //determines type of parsing and SYNTAX
    for( int i = 0; i < strlen( temp ); i ++ )
    {
        if( temp[i] == ';' && quotes == false && semi == false && colon == false )
        {
            semi = true;
            temp[i] = '\n';
        }
        if( temp[i] == ':' && quotes == false && colon == false )
        {
            //puts a space at the first : for parsing
            colon = true;
            temp[i] = '\n';
        }
        else if( temp[i] == '"' && quotes == false)
        {
            quotes = true;
        }
        else if( temp[i] == '"' && quotes == true)
        {
            quotes = false;
        }
    }
    if( colon == false )
    {
        return SYNTAX;
    }

    //parameter parsing
    if( semi == true )
    {
        int semiColon = 1;
        int equals = 0;
        int counter = 0;
        quotes = false;
        colon = false;

        for( int i = 0; i < strlen( temp ); i ++ )
        {
            if( temp[i] == ';' && quotes == false && colon == false )
            {
                semiColon ++;
            }
            else if( temp[i] == ':' && colon == false && quotes == false )
            {
                colon = true;
            }
            else if( temp[i] == '=' && quotes == false && colon == false )
            {
                //makes sure the equal signs and semi colons match up
                equals ++;
            }
            else if( temp[i] == '"' && quotes == true )
            {
                quotes = false;
            }
            else if( temp[i] == '"' && quotes == false )
            {
                quotes = true;
            }
        }
        if( semiColon != equals )
        {
            return SYNTAX;
        }

        //splits into property name, paramter text(name, value) and property value
        token = strtok( temp, "\n" );
        prop->name = malloc( sizeof(char)*(strlen(token)+1) );
        assert( prop->name != NULL );
        strcpy ( prop->name, token );

        for(int i = 0; i < strlen( prop->name ); i ++ )
        {
            if(prop->name[i] == ' ')
            {
                free( paramText );
                return SYNTAX;
            }
        }

        while( token != NULL )
        {
            token = strtok( NULL, "\n" );
            if( counter == 0 )
            {
                paramText = malloc ( sizeof(char)*(strlen(token)+1) );
                assert( paramText != NULL );
                strcpy ( paramText, token );
            }
            else if( counter == 1 )
            {
                prop->value = malloc( sizeof(char)*(strlen(token)+1) );
                assert( prop->value != NULL );
                strcpy ( prop->value, token );
            }
            counter ++;
        }

        prop->param = parseCalParam( paramText, &prop->nparams );
        for(int i = 0; i < strlen( prop->param->name ); i ++ )
        {
            if(prop->param->name[i] == ' ')
            {
                free( paramText );
                return SYNTAX;
            }
        }
        free( paramText );
    }

    //simple parsing
    else
    {
        //just property name and value without quotes
        if( temp[0] == '\n' )
        {
            return SYNTAX;
        }
        else
        {
            token = strtok( temp, "\n" );
            prop->name = malloc( sizeof(char)*(strlen(token)+1) );
            assert( prop->name != NULL );
            strcpy ( prop->name, token );

            for(int i = 0; i < strlen( prop->name ); i ++ )
            {
                if(prop->name[i] == ' ')
                {
                    return SYNTAX;
                }
            }

            token = strtok( NULL, "\n" );
            if (token != NULL)
            {
                prop->value = malloc( sizeof(char)*(strlen(token)+1) );
                assert( prop->value != NULL );
                strcpy ( prop->value, token );
            }
            else
            {
                prop->value = malloc( sizeof(char)*10 );
                assert( prop->value != NULL );
                strcpy ( prop->value, "" );
            }
        }

        prop->param = NULL;
        prop->nparams = 0;
        prop->next = NULL;
    }

    prop->name = parseCalUpperCase( prop->name );
    prop->next = NULL;

    return error;
}

/*
 returns newly allocated newCalProp structure
 Arguments: none
 Return value: CalProp structure
 */
CalParam *parseCalParam( char *buff, int *paramNum )
{
    int counter = 0;
    char *token = NULL;
    char *values = malloc ( sizeof(char)*5000 );//temp string holder
    assert( values != NULL );

    char *name =  NULL;//temp string holder
    char **temp = malloc ( sizeof(char*)*100 ); //temp array for strings
    assert( temp != NULL );

    CalParam *head = NULL;
    *paramNum = 0;

    bool quotes = false;
    for( int i = 0; i < strlen(buff); i ++ )
    {
        if( buff[i] == ';' && quotes == false )
        {
            buff[i] = '\n';
        }
        else if( buff[i] == '"' && quotes == true )
        {
            quotes = false;
        }
        else if( buff[i] == '"' && quotes == false )
        {
            quotes = true;
        }
    }

    //creates an array of strings that include param name and value
    token = strtok( buff, "\n" );
    while (token != NULL)
    {
        temp[counter] = malloc ( sizeof(char)*5000 );
        assert( temp[counter] != NULL );
        strcpy(temp[counter], token);
        token = strtok( NULL, "\n" );
        counter ++;
        *paramNum = *paramNum + 1;
    }

    //splits names and values and adds the to param list
    for ( int i = counter-1; i >= 0 ; i -- )
    {
        name = parseCalName( temp[i], values );
        head = addToCalParam( head, name, values );
        free( temp[i] );
        free( name );
    }

    free( values );
    free( temp );

    return head;
}


/*
 returns newly allocated newCalProp structure
 Arguments: none
 Return value: CalProp structure
 */
CalParam *addToCalParam( CalParam *head, char *name, char *values )
{
    CalParam *new = malloc( sizeof(CalParam) + sizeof(char) );
    assert( new != NULL );
    new->next = NULL;
    new->nvalues = 0;

    char * token = NULL;
    int counter = 0;

    new->name = malloc( sizeof(char)*(strlen(name)+1));
    assert( new->name != NULL );
    strcpy(new->name, name);
    new->name = parseCalUpperCase( new->name );
    new->next = NULL;

    bool quotes = false;
    for( int i = 0; i < strlen(values); i ++ )
    {
        if( values[i] == ',' && quotes == false )
        {
            values[i] = '\n';
        }
        else if( values[i] == '"' && quotes == true )
        {
            quotes = false;
        }
        else if( values[i] == '"' && quotes == false )
        {
            quotes = true;
        }
    }

    token = strtok( values, "\n" );
    while (token != NULL)
    {
        new = realloc( new, sizeof(CalParam) + sizeof(char*)*(counter+1) );
        new->value[counter] = malloc( sizeof(char)*(strlen(token)+1) );
        assert( new->value[counter] != NULL );
        strcpy ( new->value[counter], token );

        //makes value UPPERCASE if it is not in "quotes"
        if( strcmp( new->value[counter], new->name) == 0 )
        {
            new->value[counter] = "";
        }

        token = strtok( NULL, "\n" );
        counter ++;
    }
    new->nvalues = counter;

    new->next = head;
    return new;
}


/*
 returns newly allocated newCalProp structure
 Arguments: none
 Return value: CalProp structure
 */
char *parseCalName( char *name, char *values )
{
    char *token = NULL;
    char *temp = malloc( sizeof(char)*5000 );
    assert( temp != NULL );

    //splits string into param name and values
    token = strtok( name, "=" );
    strcpy( temp, token );
    while (token != NULL)
    {
        strcpy ( values, token );
        token = strtok( NULL, "=" );
    }
    temp = parseCalUpperCase( temp );
    return temp;
}


void freeCalComp( CalComp *const comp )
{
    //free comp
    free( comp->name );
    comp->nprops = 0;
    comp->ncomps = 0;

    //free comp properties
    while( comp->prop != NULL )
    {
        free( comp->prop->name );
        free( comp->prop->value );
        comp->prop->nparams = 0;

        //free paramaters
        while( comp->prop->param != NULL )
        {
            free( comp->prop->param->name );
            comp->prop->param->nvalues = 0;

            //free param values
            for( int i = 0; i <= comp->prop->param->nvalues; i ++ )
            {
                free( comp->prop->param->value[i] );
            }
            comp->prop->param = comp->prop->param->next;
        }
        free( comp->prop->param );
        comp->prop = comp->prop->next;
    }
    free( comp->prop );

    for( int i = 0; i < comp->ncomps; i ++ )
    {
        freeCalComp( comp->comp[i] );
        free( comp->comp[i] );
    }
    /*if( comp->ncomps > 1 )
    {
        //frees the extra comp malloced
        free( comp->comp[comp->ncomps+1] );
    }*/

    free( comp );
}


/*
 returns freed prop struct allocated in readCalComp
 Arguments: prop
 Return value: freed CalProp structure
 */
void freeCalProp( CalProp *prop )
{
    free(prop->name);
    free(prop->value);
    prop->nparams = 0;
    prop->param = NULL;
    prop->next = NULL;
    free(prop);
    prop = NULL;
}


/*
 Checks Components version number and prodid to see if they
 are valid.
 Arguments: Calcomp structure
 Return value: error code
 */
CalError checkVerId( CalComp * comp )
{
    //counts number of versions and id numbers found
    int ver = 0;
    int id = 0;
    //stores codes for each version and id error
    CalError verError = OK;
    CalError idError = OK;
    CalProp * prop = comp->prop;

    while( prop != NULL )
    {
        //checks version number
        if( strcmp(prop->name, "VERSION") == 0 && strcmp(prop->value, VCAL_VER) == 0 )
        {
            verError = OK;
        }
        else if( strcmp(prop->name, "VERSION") == 0 && strcmp(prop->value, VCAL_VER) != 0 )
        {
            verError = BADVER;
        }

        //checks number of version and prodid
        if( strcmp(prop->name, "VERSION") == 0 )
        {
            ver ++;
        }
        else if( strcmp(prop->name, "PRODID") == 0 )
        {
            id ++;
        }

        //returns error if there is 0 or more than 1 version
        if( ver == 0  || ver > 1 )
        {
            verError = BADVER;
        }

        //returns error if there is 0 or more than 1 id
        if( id == 0 || id > 1 )
        {
            idError = NOPROD;
        }
        else
        {
            idError = OK;
        }

        prop = prop->next;
    }

    //delagates errors
    if( (verError == BADVER && idError == NOPROD) || comp->nprops == 0 )
    {
        return BADVER;
    }
    else if( verError == OK && idError != OK )
    {
        return idError;
    }
    else if( verError == OK && idError == OK )
    {
        return OK;
    }
    else
    {
        return verError;
    }

    return OK;
}

/*
 Checks Components name for a 'V' at the first
 letter and number of components is not 0.
 Calls recursively
 Arguments: Calcomp structure
 Return value: error code
 */
int checkCompName( CalComp * comp )
{
    static int vComp = 0;

    if( comp->name[0] == 'V' || comp->name[0] == 'v' )
    {
        vComp ++;
    }

    for( int i = 0; i < comp->ncomps; i ++ )
    {
        checkCompName( comp->comp[i] );
    }

    return vComp-1;
}


/*
 Changes string to all CAPS
 Arguments: pointer to a string
 Return value: pointer to a string with all CAPS
 */
char *parseCalUpperCase( char *str )
{
    for( int i = 0; i < strlen( str ); i ++ )
    {
        if( islower( str[i]) != 0 )
        {
            str[i] = toupper(str[i]);
        }
    }
    return str;
}


/*
 checks if first and last letter are quatations
 Arguments: pointer to a sting
 Return value: boolean, true if string
 is enclosed in qoutes.
 */
bool parseCalQuotes( char *str )
{
    if(str[0] == '"' && str[strlen(str)-1] == '"')
    {
        return true;
    }
    return false;
}


//        ***Testing Function***

/*
 Same idea as free function, just prints instead
 */
void printCalComp( CalComp *comp )
{
    printf( "compName:%s\n", comp->name );
    printf( "nprops:%d\n", comp->nprops );
    printf( "ncomps:%d\n", comp->ncomps );

    while( comp->prop != NULL )
    {
        printf("propName:%s\n", comp->prop->name);
        printf("propValue:%s\n", comp->prop->value);
        printf("propParam:%d\n", comp->prop->nparams);
        while( comp->prop->param != NULL )
        {
            printf("paramName:%s\n", comp->prop->param->name);
            printf("paramNvalues:%d\n", comp->prop->param->nvalues);
            for( int i = 0; i < comp->prop->param->nvalues; i ++ )
            {
                printf("param:%s\n", comp->prop->param->value[i]);
            }
            comp->prop->param = comp->prop->param->next;
        }
        comp->prop = comp->prop->next;
    }

    for( int i = 0; i < comp->ncomps; i ++ )
    {
        printCalComp( comp->comp[i] );
    }
}
