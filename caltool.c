/************************************************************
 caltool.c -- Public interface for iCalendar tools in caltool.c

 Created: Jan.13 2016
 Author: Nick Major #0879292
 Contact: nmajor@mail.uoguelph.ca

 ***********************************************************/

#include "caltool.h"

typedef struct tm tm;//gets rid of 'struct'

bool checkTime( CalProp *prop, int size, tm *firstTime, tm *lastTime);
int cmpString ( const void *str1, const void *str2 );
int cmpTime ( const void *event1, const void *event2 );
void Remove (int position, CalProp * L, int size);
bool withinRange( CalProp *prop, int size, time_t from, time_t to );
extern CalProp *addToCalProp( CalProp *head, CalProp *add, int size );
int getDateErr ( char *argv[], int errorcode );

//event struct for sorting
typedef struct extractEvent {
    char summary[50];
    time_t time;
} extractEvent;


int main( int argc, char * argv[] )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = error.lineto = 0;

    //File input and output
    FILE *icsFile = stdin;
    FILE *write = stdout;

    //pcomp struct
    CalComp * pcomp = NULL;

    //Calls info
    if( argc == 1 )
    {
        fprintf( stderr, "missing parameter in command\n" );
        return EXIT_FAILURE;
    }
    else if( strcmp( argv[1], "-info" ) == 0 )
    {
        if( argc > 2 )
	{
            fprintf( stderr, "too many parameter in command\n" );
            return EXIT_FAILURE;
        }
        error = readCalFile( icsFile, &pcomp );
        if( error.code != OK )
        {
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        error = calInfo( pcomp, error.lineto, write );
        if( error.code != OK )
        {
            freeCalComp( pcomp );
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }


        freeCalComp( pcomp );
    }

    //calls extract
    else if( strcmp( argv[1], "-extract" ) == 0 )
    {
        if( argc > 3 )
	       {
            fprintf( stderr, "too many parameter in command\n" );
            return EXIT_FAILURE;
        }

        error = readCalFile( icsFile, &pcomp );
        if( error.code != OK )
        {
          fprintf( stderr, "Error Code: %d\n", error.code );
          fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        //-e for events
        if( strcmp( argv[2], "e" ) == 0 )
        {
            error = calExtract( pcomp, OEVENT, write );
        }

        //-x for properties
        else if( strcmp( argv[2], "x" ) == 0 )
        {
            error = calExtract( pcomp, OPROP, write );
        }

        //anything else is an error
        else
        {
            fprintf( stderr, "Not a valid parameter\n" );
            return EXIT_FAILURE;
        }

        if( error.code != OK )
        {
            freeCalComp( pcomp );
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        freeCalComp( pcomp );
    }

    //calls filter
    else if( strcmp( argv[1], "-filter" ) == 0 )
    {
        if( argc > 7 )
	{
            fprintf( stderr, "too many parameter in command\n" );
            return EXIT_FAILURE;
        }
        time_t from = 0;
        time_t to = 0;
        tm temp = {0};
        char fromDate[80];
        char toDate[80];
	       int errorcode = 0;

        //Simple, no date range
        if( argc == 3 )
        {
            error = readCalFile( icsFile, &pcomp );
            if( error.code != OK )
            {
              fprintf( stderr, "Error Code: %d\n", error.code );
              fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
                return EXIT_FAILURE;
            }
        }

        //only from or to date is included
        else if ( argc == 5 )
        {
            if( icsFile == NULL )
            {
                error.code = IOERR;
                fprintf( stderr, "Error Code: %d\n", error.code );
                fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
                return EXIT_FAILURE;
            }

            error = readCalFile( icsFile, &pcomp );
            if( error.code != OK )
            {
              fprintf( stderr, "Error Code: %d\n", error.code );
              fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
                return EXIT_FAILURE;
            }

            //from date or to date
            if( strcmp( argv[4], "today" ) == 0 )
            {
                //sets to current date
                from = time(NULL);
                temp = *localtime(&from);
            }
            if( strcmp( argv[3], "from" ) == 0 )
            {
	      errorcode = getdate_r( argv[4], &temp );
	      if( getDateErr(argv, errorcode) == 1 )
                {
                    return EXIT_FAILURE;
                }

                //sets from date
                temp.tm_sec = temp.tm_min = temp.tm_hour = 0;
                temp.tm_isdst = -1;
                from = mktime( &temp );
                strftime( fromDate, 80, "%Y-%b-%d", &temp );

                //sets to over 100 years ahead
                temp.tm_year = 350;
                to = mktime( &temp );
                strftime( toDate, 80, "%Y-%b-%d", &temp );
            }
            else if( strcmp( argv[3], "to" ) == 0 )
            {
	      errorcode = getdate_r( argv[4], &temp );
	      if( getDateErr(argv, errorcode) == 1 )
                {
                    return EXIT_FAILURE;
                }

                //sets to
                temp.tm_sec = 0;
                temp.tm_min = 59;
                temp.tm_hour = 23;
                temp.tm_isdst = -1;
                to = mktime( &temp );
                strftime( toDate, 80, "%Y-%b-%d", &temp );

                //sets from to 1970
                temp.tm_sec = temp.tm_min = temp.tm_hour = temp.tm_year = temp.tm_mday = temp.tm_yday = 0;
                temp.tm_isdst = -1;
                from = 0;
                strftime( fromDate, 80, "%Y-%b-%d", &temp );
            }
            else
            {
                fprintf( stderr, "'%s' should be 'to' or 'from'\n", argv[3]);
                return EXIT_FAILURE;
            }
        }

        //date range included
        else if ( argc == 7 )
        {
            error = readCalFile( icsFile, &pcomp );
            if( error.code != OK )
            {
                fprintf( stderr, "%d\n", error.code );
                fprintf( stderr, "%d-%d\n", error.linefrom, error.lineto );
                return EXIT_FAILURE;
            }

            //from date
            if( strcmp( argv[4], "today" ) == 0 && strcmp( argv[3], "from" ) == 0  )
            {
                //sets to current date
                from = time(NULL);
                temp = *localtime(&from);
            }
            else if( strcmp( argv[3], "from" ) == 0 )
            {
	      errorcode = getdate_r( argv[4], &temp);
	      if( getDateErr(argv, errorcode) == 1 )
                {
                    return EXIT_FAILURE;
                }
            }
            else
            {
                fprintf( stderr, "'%s' should be 'from'\n", argv[3]);
                return EXIT_FAILURE;
            }

            //removes hours, min, seconds
            temp.tm_sec = temp.tm_min = temp.tm_hour = 0;
            temp.tm_isdst = -1;
            from = mktime( &temp );
            strftime( fromDate, 80, "%Y-%b-%d", &temp );


            //to date
            if( strcmp( argv[6], "today" ) == 0 && strcmp( argv[5], "to" ) == 0  )
            {
                to = time(NULL);
                temp = *localtime(&to);
            }
            else if( strcmp( argv[5], "to" ) == 0 )
            {
	        errorcode = getdate_r( argv[6], &temp);
                if( getDateErr(argv, errorcode) == 1 )
                {
                    return EXIT_FAILURE;
                }
            }
            else
            {
                fprintf( stderr, "'%s' should be 'to'\n", argv[3]);
                return EXIT_FAILURE;
            }

            //adds min and hour, removes seconds
            temp.tm_sec = 0;
            temp.tm_min = 59;
            temp.tm_hour = 23;
            temp.tm_isdst = -1;
            to = mktime( &temp );
            strftime( toDate, 80, "%Y-%b-%d", &temp );
        }
        else
        {
            fprintf( stderr, "missing or have too many parameters in command\n" );
            return EXIT_FAILURE;
        }

	if( difftime(from, to ) > 0 )
        {
            fprintf( stderr, "%s is not earlier than %s\n", fromDate, toDate );
            return 1;
	}


        //events or todos
        if( strcmp( argv[2], "e" ) == 0 )
        {
            error = calFilter( pcomp, OEVENT, from, to, write );
        }
        else if( strcmp( argv[2], "t" ) == 0 )
        {
            error = calFilter( pcomp, OTODO, from, to, write );
        }
        else
        {
            freeCalComp( pcomp );
            fprintf( stderr, "Not a valid parameter\n" );
            return EXIT_FAILURE;
        }

        if( error.code != OK )
        {
            freeCalComp( pcomp );
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        freeCalComp( pcomp );
    }

    //calls combine
    else if( strcmp( argv[1], "-combine" ) == 0 && argc > 1 )
    {
        if( icsFile == NULL )
        {
            error.code = IOERR;
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        //reads first file
        error = readCalFile( icsFile, &pcomp );
        if( error.code != OK )
        {
          fprintf( stderr, "Error Code: %d\n", error.code );
          fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        //reads second file
        FILE *new = fopen( argv[2], "r" );
        CalComp * pcomp2 = NULL;

	       if( new == NULL )
        {
            error.code = IOERR;
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        error = readCalFile( new, &pcomp2 );
        if( error.code != OK )
        {
          fprintf( stderr, "Error Code: %d\n", error.code );
          fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        fclose( new );

        //combines file
        error = calCombine( pcomp, pcomp2, write );
        if( error.code != OK )
        {
            freeCalComp( pcomp );
            freeCalComp( pcomp2 );
            fprintf( stderr, "Error Code: %d\n", error.code );
            fprintf( stderr, "Lines: %d-%d\n", error.linefrom, error.lineto );
            return EXIT_FAILURE;
        }

        freeCalComp( pcomp );
        freeCalComp( pcomp2 );
    }

    //returns faliure if errors
    else
    {
        fprintf( stderr, "not a valid command\n" );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


int getDateErr ( char *argv[], int errorcode )
{
    //error codes
    if( errorcode == 1 || errorcode == 2 || errorcode == 3 || errorcode == 4 || errorcode == 5)
    {
        fprintf( stderr, "Problem with DATEMSK environment variable or template file (error codes 1-5)\n");
        return 1;
    }
    else if( errorcode == 6 )
    {
        fprintf( stderr, "Memory allocation failed\n");
        return 1;
    }
    else if( errorcode == 7 || errorcode == 8 )
    {
        fprintf( stderr, "Date could not be interpreted\n" );
        return 1;
    }

    return 0;
}


CalStatus calInfo( const CalComp *comp, int lines, FILE *const txtfile )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = error.lineto = 0;

    //temp string
    char temp[1000] = "";
    char buff1[80] = "";
    char buff2[80] = "";

    tm firstTime = {0};
    tm lastTime = {0};
    int components = comp->ncomps;

    bool depth1 = false;
    bool depth2 = false;

    //counters
    int sub = 0;
    int events = 0;
    int todo = 0;

    int props = comp->nprops;
    CalProp *prop = NULL;
    char CN[500][50];
    int index = 0;

    //intializes check time
    checkTime( NULL, 0, &firstTime, &lastTime );

    for( int i = 0; i < components; i ++ )
    {
        //second depth
        if( strcmp( comp->comp[i]->name, "VEVENT" ) == 0 )
        {
            events ++;
        }
        else if( strcmp( comp->comp[i]->name, "VTODO" ) == 0 )
        {
            todo ++;
        }

        props = props + comp->comp[i]->nprops;
        prop = comp->comp[i]->prop;

        for( int x = 0; x < comp->comp[i]->nprops; x ++ )
        {
            if( strcmp( prop->name, "ORGANIZER" ) == 0 )
            {
                strcpy( CN[index], prop->param->value[0] );
                index ++;
            }
            prop = prop->next;
        }

        if( strcmp(comp->comp[i]->name, "VTIMEZONE") != 0 )
        {
          depth1 = checkTime( comp->comp[i]->prop, comp->comp[i]->nprops, &firstTime, &lastTime );
        }

        //third depth
        for( int j = 0; j < comp->comp[i]->ncomps; j ++ )
        {
            if( strcmp( comp->comp[i]->comp[j]->name, "VEVENT" ) == 0 )
            {
                events ++;
            }
            else if( strcmp( comp->comp[i]->comp[j]->name, "VTODO" ) == 0 )
            {
                todo ++;
            }

            sub ++;
            props = props + comp->comp[i]->comp[j]->nprops;
            prop = comp->comp[i]->comp[j]->prop;

            for( int x = 0; x < comp->comp[i]->comp[j]->nprops; x ++ )
            {
                if( strcmp( prop->name, "ORGANIZER" ) == 0 )
                {
                    strcpy( CN[index], prop->param->value[0] );
                    index ++;
                }
                prop = prop->next;
            }

            if( strcmp(comp->comp[i]->comp[j]->name, "VTIMEZONE") != 0 )
            {
              depth2 = checkTime( comp->comp[i]->comp[j]->prop, comp->comp[i]->comp[j]->nprops, &firstTime, &lastTime );
            }

        }
    }

    //sorts organizers array alphabetically
    qsort( CN, index, 50, cmpString );

    //All the info printing, if statements for grammer
    //lines
    if( lines == 1 )
    {
        sprintf( temp, "%d line\n", lines);
        fputs( temp, txtfile );
    }
    else
    {
        sprintf( temp, "%d lines\n", lines);
        fputs( temp, txtfile );
    }

    //%d components: %d events, %d todos, %d others\r\n
    if( components == 1 )
    {
        sprintf( temp, "%d component:", components );
    }
    else
    {
        sprintf( temp, "%d components:", components );
    }
    if( events == 1 )
    {
        sprintf( temp, "%s %d event,", temp, events );
    }
    else
    {
        sprintf( temp, "%s %d events,", temp, events );
    }
    if( todo == 1 )
    {
        sprintf( temp, "%s %d todo,", temp, todo );
    }
    else
    {
        sprintf( temp, "%s %d todos,", temp, todo );
    }
    if( (components-events-todo) == 1 )
    {
        sprintf( temp, "%s %d other\n", temp, (components-events-todo) );
    }
    else
    {
        sprintf( temp, "%s %d others\n", temp, (components-events-todo) );
    }
    fputs( temp, txtfile );

    //sub components
    if( sub == 1 )
    {
        sprintf( temp, "%d subcomponent\n", sub );
        fputs( temp, txtfile );
    }
    else
    {
        sprintf( temp, "%d subcomponents\n", sub );
        fputs( temp, txtfile );
    }

    //totlal properties
    if( props == 1 )
    {
        sprintf( temp, "%d property\n", props );
        fputs( temp, txtfile );
    }
    else
    {
        sprintf( temp, "%d properties\n", props );
        fputs( temp, txtfile );
    }

    //date range
    //converts dates struct to proper printing form (string)
    if( depth1 == true || depth2 == true )
    {
        strftime( buff1, 80, "%Y-%b-%d", &firstTime );
        strftime( buff2, 80, "%Y-%b-%d", &lastTime );
        sprintf( temp, "From %s to %s\n", buff1, buff2 );
    }
    else
    {
        sprintf( temp, "No dates\n" );
    }
    fputs( temp, txtfile );

    //Organizer list
    if( index == 0 )
    {
        fputs( "No organizers\n", txtfile );
    }
    else
    {
        fputs( "Organizers:\n", txtfile );
        for( int z = 0; z < index; z ++ )
        {
            if( strcmp( CN[z], CN[z+1]) != 0 )
            {
                sprintf( temp, "%s\n", CN[z] );
                fputs( temp, txtfile );
            }
        }
    }

    return error;
}


/*
 compares string for qsort
 Arguments: string 1, and string 2
 Return value: strcmp, 0 if equal, not 0 if not
 */
int cmpString (const void * str1, const void * str2)
{
  return strcmp( (char *)str1, (char *)str2 );
}


/*
 compares time in event struct for qsort
 Arguments: struct 1, and struct 2
 Return value: difference of time, 0 if equal, not 0 if not
 */
int cmpTime (const void * event1, const void * event2 )
{
    extractEvent e1 = *(extractEvent *)event1;
    extractEvent e2 = *(extractEvent *)event2;

    return difftime( e1.time, e2.time );
}


/*
 looks in property list for dates and adds them to first time struct or
    last time struct if greater or less than current first and time struct
 Arguments: property to check, size of list, first and last time struct
 Return value: first and last date found (range)
 */
bool checkTime( CalProp *prop, int size, tm *firstTime, tm *lastTime)
{
    //static variables for keeping track of time as function is called in a loop
    static tm first = {0};
    static tm last = {0};
    static time_t compareFirst = 0;
    static time_t compareLast = 0;
    static bool initial = false;

    //resets static variables
    if( prop == NULL )
    {
        first.tm_hour = first.tm_isdst = first.tm_mday = first.tm_min = first.tm_mon = first.tm_sec = first.tm_wday = 0;
        last.tm_hour = last.tm_isdst = last.tm_mday = last.tm_min = last.tm_mon = last.tm_sec = last.tm_wday = 0;
        compareFirst = 0;
        compareLast = 0;
        initial = false;
    }
    else
    {
        tm temp = {0};
        char *check;
        time_t tempCompare = 0;

        //Types of property dates allowed
        char compare[9][15] = { "COMPLETED", "DTSTAMP", "DTEND", "DUE",
                                "DTSTART", "CREATED", "LAST-MODIFIED" };

        for( int i = 0; i < size; i ++ )
        {
            for( int j = 0; j < 8; j ++ )
            {
                if( strcmp( prop->name, compare[j] ) == 0 )
                {
                    //coverts property value
                    check = strptime( prop->value, "%Y%m%dT%H%M%S", &temp );
                    tempCompare = mktime( &temp );
                    //sets initial values
                    if( initial == false && check != NULL )
                    {
                        first = temp;
                        last = temp;
                        compareFirst = mktime( &temp );
                        compareLast = mktime( &temp );
                        initial = true;
                    }

                    //compares time and replaces if less than or greater than current
                    if( difftime(tempCompare, compareFirst) < 0 && initial == true )
                    {
                        first = temp;
                        compareFirst = tempCompare;
                    }
                    else if( difftime(tempCompare, compareLast) > 0 && initial == true )
                    {
                        last = temp;
                        compareLast = tempCompare;
                    }
                }
            }
            prop = prop->next;
        }

        //returns range
        *firstTime = first;
        *lastTime = last;
    }

    return initial;
}


CalStatus calExtract( const CalComp *comp, CalOpt kind, FILE *const txtfile )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = error.lineto = 0;

    CalProp *prop;//temp prop struct
    int index = 0;//temp array index
    char print[100];//temp string for printing to file


    //properties
    if( kind == OPROP )
    {
      char buff[500][50];

       //checks 1st depth
            prop = comp->prop;
            for( int x = 0; x < comp->nprops; x ++ )
            {
                if( prop->name[0] == 'X' && prop->name[1] == '-' )
                {
		    strcpy( buff[index], prop->name );
                    index ++;
                }
                prop = prop->next;
            }

        for( int i = 0; i < comp->ncomps; i ++ )
        {
            //checks 2nd depth
            prop = comp->comp[i]->prop;
            for( int x = 0; x < comp->comp[i]->nprops; x ++ )
            {
                if( prop->name[0] == 'X' && prop->name[1] == '-' )
                {
		    strcpy( buff[index], prop->name );
                    index ++;
                }
                prop = prop->next;
            }

            //checks 3rd depth
            for( int j = 0; j < comp->comp[i]->ncomps; j ++ )
            {
                prop = comp->comp[i]->comp[j]->prop;
                for( int x = 0; x < comp->comp[i]->comp[j]->nprops; x ++ )
                {
                    if( prop->name[0] == 'X' && prop->name[1] == '-' )
                    {;
		         strcpy( buff[index], prop->name );
                         index ++;
                    }
                    prop = prop->next;
                }
            }
        }

        //sorts array of 'X-' properties
        qsort(buff, index, sizeof(buff[0]), cmpString );

        //prints sorted array
        for( int z = 0; z < index; z ++ )
        {
	  if( strcmp( buff[z], buff[z+1]) != 0 )
	  {
                sprintf( print, "%s\n", buff[z] );
                fputs( print, txtfile );
	  }
        }
    }

    //events
    else
    {
        //temp time variables for printing
        tm temp = {0};
        char time[50];

        extractEvent event[500];//temp array for summary and time

        for( int i = 0; i < comp->ncomps; i ++ )
        {
            //checks 1st depth for VEVENT
            if( strcmp( comp->comp[i]->name, "VEVENT") == 0 )
            {
                prop = comp->comp[i]->prop;
                while( prop != NULL )
                {
                    //looking for DTSTART and SUMMARY
                    if( strcmp( prop->name, "DTSTART" ) == 0 )
                    {
                        strptime( prop->value, "%Y%m%dT%H%M%S", &temp );
                        event[index].time = mktime( &temp );
                    }
                    else if( strcmp( prop->name, "SUMMARY" ) == 0 )
                    {
                        strcpy( event[index].summary, prop->value );
                    }
                    else if( strcmp( event[index].summary, "" ) == 0 )
                    {
                        strcpy( event[index].summary, "(na)" );
                    }

                    prop = prop->next;
                }
		if( event[index].time != 0 )
	        {
                    index ++;
		}
            }
        }

        //sorts event array
        qsort( event, index, sizeof( extractEvent ), cmpTime );

        //prints sorted array
        for( int z = 0; z < index; z ++ )
        {
            temp = *localtime( &event[z].time );
            strftime( time, 80, "%Y-%b-%d %l:%M %p",  &temp );
            sprintf( print, "%s: %s\n", time, event[z].summary );
            fputs( print, txtfile );
        }
    }

    return error;
}


CalStatus calFilter( const CalComp *comp, CalOpt content, time_t datefrom, time_t dateto, FILE *const icsfile )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = error.lineto = 0;

    CalComp *copyComp = malloc( sizeof(CalComp) + sizeof(CalComp*)*(comp->ncomps+1) );
    assert( copyComp != NULL );

    memcpy( copyComp, comp, sizeof(CalComp) + sizeof(CalComp*)*(comp->ncomps+1) );//shallow copy
    char component[10];//which type of component searching for

    //Event or Todo component
    if( content == OEVENT )
    {
        strcpy( component, "VEVENT" );
    }
    else
    {
        strcpy( component, "VTODO" );
    }

    //if no date range specified
    if( datefrom == 0 && dateto == 0 )
    {
        for( int i = 0; i < copyComp->ncomps; i ++ )
        {
            if( strcmp(copyComp->comp[i]->name, component ) != 0 )
            {
                //moves array up
                for( int j = i; j < copyComp->ncomps-i; j ++ )
                {
                    copyComp->comp[j] = copyComp->comp[j+1];
                }
                copyComp->ncomps --;
                i --;
            }
        }
    }

    //date range specified
    else if( datefrom != 0 || dateto != 0 )
    {
        for( int i = 0; i < copyComp->ncomps; i ++ )
        {
            //removes components without matching name
            if( strcmp(copyComp->comp[i]->name, component ) != 0 )
            {
                for( int j = i; j < copyComp->ncomps-i; j ++ )
                {
                    copyComp->comp[j] = copyComp->comp[j+1];
                }
                copyComp->ncomps --;
                i --;
            }

            //checks dates for matching names
            else
            {
                //looks at 2nd depth if there isnt a 3rd depth
                if( copyComp->comp[i]->ncomps == 0 || copyComp->comp[i]->ncomps < 1 )
                {
                    if( withinRange( copyComp->comp[i]->prop, copyComp->comp[i]->nprops, datefrom, dateto ) == false )
                    {
                        for( int j = i; j < copyComp->ncomps-i; j ++ )
                        {
                            copyComp->comp[j] = copyComp->comp[j+1];
                        }

                        copyComp->ncomps --;
                        i --;
                    }
                }
                //looks in the 2nd and 3rd depth for dates
                else
                {
                    for( int k = 0; k < copyComp->comp[i]->ncomps-1; k ++ )
                    {
                        if( withinRange( copyComp->comp[i]->prop, copyComp->comp[i]->nprops, datefrom, dateto ) == false
                            && withinRange( copyComp->comp[i]->comp[k]->prop, copyComp->comp[i]->comp[k]->nprops, datefrom, dateto ) == false )
                        {
                            for( int j = i; j < copyComp->ncomps-i; j ++ )
                            {
                                copyComp->comp[j] = copyComp->comp[j+1];
                            }

                            copyComp->ncomps --;
                            i --;
                        }
                    }
                }

            }
        }
    }

    //NOCAL or print to file
    if( copyComp->ncomps == 0 )
    {
        error.code = NOCAL;
    }
    else
    {
        error = writeCalComp( icsfile, copyComp );
    }

     free( copyComp );

    return error;
}


/*
 checks property list for 7 valid date prop values if within range
 Arguments: property list, size of list, range of dates
 Return value: true if within range
 */
bool withinRange( CalProp *prop, int size, time_t from, time_t to )
{
    tm temp = {0};
    char *check;
    time_t tempCompare = 0;

    //valid date properties
    char compare[8][15] = { "COMPLETED", "DTSTART", "DTEND", "DUE" };

    for( int i = 0; i < size && prop != NULL; i ++ )
    {
        for( int j = 0; j < 7; j ++ )
        {
            if( strcmp( prop->name, compare[j] ) == 0 )
            {
                check = strptime( prop->value, "%Y%m%dT%H%M%S", &temp );
                temp.tm_isdst = -1;
                tempCompare = mktime( &temp );

                //returns true if within range
                if( tempCompare <= to && tempCompare >= from && check != NULL )
                {
                    return true;
                }
            }
        }
        prop = prop->next;
    }

    return false;
}


CalStatus calCombine( const CalComp *comp1, const CalComp *comp2, FILE *const icsfile )
{
    CalStatus error;
    error.code = OK;
    error.linefrom = error.lineto = 0;

    int counter = 0;
    CalProp *newProp = malloc( sizeof( CalProp ) );
    newProp->next = NULL;

    //new comp for combining comp1 and comp2
    CalComp *newComp = malloc( sizeof(CalComp) + sizeof(CalComp*)*(comp1->ncomps+1+comp2->ncomps) );
    assert( newComp != NULL );

    memcpy( newComp, comp1, sizeof(CalComp) + sizeof(CalComp*)*(comp1->ncomps+1) );

    //copy of comp2
    CalComp *copy2 = malloc( sizeof(CalComp) + sizeof(CalComp*)*(comp2->ncomps+1) );
    assert( copy2 != NULL );

    memcpy( copy2, comp2, sizeof(CalComp) + sizeof(CalComp*)*(comp2->ncomps+1) );

    //search comp2 and adds properties that aren't PROID/VERSION
    CalProp *prop = copy2->prop;
    while( prop != NULL )
    {
        if( strcmp( prop->name, "PRODID" ) != 0 && strcmp( prop->name, "VERSION" ) != 0)
        {
            addToCalProp( newComp->prop, prop, counter );
        }
        counter ++;
        prop = prop->next;
    }

    //increases nprops and switches over combined properties
    newComp->nprops = newComp->nprops + counter;

    //copies over all components
    for( int i = 0; i < copy2->ncomps; i ++ )
    {
        newComp->comp[newComp->ncomps + i] = copy2->comp[i];
    }

    newComp->ncomps = newComp->ncomps + copy2->ncomps;


    //prints to file
    error = writeCalComp( icsfile, newComp );

    free ( newComp );
    free ( copy2 );

    return error;
}
