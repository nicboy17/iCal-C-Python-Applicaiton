/***************************************************************************
 calmodule.c -- calutil.c python wrapper functions for extending C.

 Created: Mar.07 2016
 Author: Nick Major #0879292
 Contact: nmajor@mail.uoguelph.ca
***************************************************************************/

#include <Python.h>
#include "calutil.h"
#include <stdbool.h>
#include <ctype.h>
#include <strings.h>
#include <assert.h>
#include <time.h>

static PyObject *Cal_readFile( PyObject *self, PyObject *args );
static PyObject *Cal_writeFile( PyObject *self, PyObject *args );
static PyObject *Cal_freeFile( PyObject *self, PyObject *args );
void buildTupleList( CalComp *comp,  PyObject *result );
CalStatus removeTodo( CalComp *comp, int remove[], int size, FILE *pyFile);
static PyObject *Cal_storeFile( PyObject *self, PyObject *args );

//struct for file viwer
typedef struct tupleInfo{
    char name[50];
    int nprops;
    int ncomps;
    char summary[50];
}tupleInfo;

//struct for table data
typedef struct tupleTable{
    char type[60];
    char name[60]; //CN parameter
    char contact[60]; //mailto:
    char summary[60]; //organizer property
    char startTime[60];
    char location[60]; //property, can be NULL
    int priority; //property, can be NULL
}tupleTable;


//***NEW***
/*
 Builds a tupe list of data Needed for SQL database tables
 Arguments: CalComp structure and list to return
 Return value: returns Py_List object
*/
static PyObject *Cal_storeFile(PyObject *self, PyObject *args)
{
  CalStatus error;
  error.code = OK;
  error.linefrom = 0;
  error.lineto = 0;

  CalComp *comp = NULL;

  char *filename = NULL;
  PyObject *result = NULL;
  FILE *pyFile = NULL;

  PyArg_ParseTuple( args, "sO", &filename, &result );

  pyFile = fopen( filename, "r" );

  error = readCalFile( pyFile, &comp );
  if( error.code != OK )
  {
    char temp[100];
    sprintf(temp, "File Name: %s\nError Code: %d\nLines read: %d\n", filename, error.code, error.lineto);
    return Py_BuildValue( "s", temp );
  }

  fclose( pyFile );

  CalProp * prop = NULL;
  tupleTable tuple;

  strcpy(tuple.type, "");
  strcpy(tuple.name, "");
  strcpy(tuple.contact, "");
  strcpy(tuple.summary, "");
  strcpy(tuple.startTime, "");
  strcpy(tuple.location, "");
  tuple.priority = 0;

  for( int i = 0; i < comp->ncomps; i ++ )
  {
      strcpy( tuple.type, comp->comp[i]->name );

      prop = comp->comp[i]->prop;

      for( int x = 0; x < comp->comp[i]->nprops; x ++ )
      {
          if( strcmp( prop->name, "SUMMARY" ) == 0 )
          {
              strcpy( tuple.summary, prop->value );
          }

          if( strcmp( prop->name, "ORGANIZER" ) == 0 )
          {
              strcpy( tuple.contact, prop->value );
          }

          if( strcmp( prop->name, "DTSTART" ) == 0 )
          {
              char buff[80];
              struct tm temp = {0};
              strptime( prop->value, "%Y%m%dT%H%M%S", &temp );
              strftime( buff, 80, "%Y-%m-%d %H:%M:%S", &temp );
              strcpy( tuple.startTime, buff );
          }

          if( strcmp( prop->name, "LOCATION" ) == 0 )
          {
              strcpy( tuple.location, prop->value );
          }

          if( strcmp( prop->name, "PRIORITY" ) == 0 )
          {
              tuple.priority = atoi(prop->value);
          }

          for( int j = 0; j < prop->nparams; j ++ )
          {
            if( strcmp( prop->param->name, "CN" ) == 0 )
            {
                strcpy( tuple.name, prop->param->value[0] );
            }
          }

          prop = prop->next;
      }

    PyList_Append( result, Py_BuildValue( "(ssssssi)", tuple.type, tuple.name, tuple.contact, tuple.summary, tuple.startTime, tuple.location, tuple.priority ) );
  }

  freeCalComp( comp );

  return Py_BuildValue( "s", "OK" );
}

/*
 Builds a tupe list for gui file viewer
 Arguments: CalComp structure and list to return
 Return value: returns Py_List object
*/
void buildTupleList( CalComp *comp,  PyObject *result )
{
    CalProp * prop = NULL;
    tupleInfo tuple;

    strcpy(tuple.name, "");
    strcpy(tuple.summary, "");
    tuple.ncomps = 0;
    tuple.nprops = 0;

    for( int i = 0; i < comp->ncomps; i ++ )
    {
        strcpy( tuple.name, comp->comp[i]->name );
        tuple.nprops = comp->comp[i]->nprops;
        tuple.ncomps = comp->comp[i]->ncomps;

        prop = comp->comp[i]->prop;

        for( int x = 0; x < comp->comp[i]->nprops; x ++ )
        {
            if( strcmp( prop->name, "SUMMARY" ) == 0 )
            {
                strcpy( tuple.summary, prop->value );
                break;
            }
            else if( x ==  comp->comp[i]->nprops-1 )
            {
                strcpy( tuple.summary, "" );
            }
            prop = prop->next;
        }
      PyList_Append( result, Py_BuildValue( "(siis)", tuple.name, tuple.nprops, tuple.ncomps, tuple.summary ) );
    }
}

/*
 Case where compList is less than ncomps, removes deselected Todo
 Arguments: CalComp structure, array of todos to be removed
 Return value: returns updates CalComp structure without deleted todo
*/
CalStatus removeTodo( CalComp *comp, int remove[], int size, FILE *pyFile )
{
  CalStatus error;
  error.code = OK;
  error.linefrom = 0;
  error.lineto = 0;

  CalComp *newComp = malloc( sizeof(CalComp) + sizeof(CalComp*)*(size+1) );
  assert( newComp != NULL );

  newComp->name = comp->name;
  newComp->prop = comp->prop;
  newComp->nprops = comp->nprops;

  for( int i = 0; i < size; i ++ )
  {
      newComp->comp[i] = comp->comp[remove[i]-1];
  }

  newComp->ncomps = size;

  writeCalComp( NULL, NULL );
  error = writeCalComp( pyFile, newComp );

    return error;
}


static PyObject *Cal_readFile(PyObject *self, PyObject *args)
{
  CalStatus error;
  error.code = OK;
  error.linefrom = 0;
  error.lineto = 0;

  CalComp *pcal = NULL;

  char *filename = NULL;
  PyObject *result = NULL;
  FILE *pyFile = NULL;

  PyArg_ParseTuple( args, "sO", &filename, &result );

  pyFile = fopen( filename, "r" );

  error = readCalFile( pyFile, &pcal );
  if( error.code != OK )
  {
    char temp[100];
    sprintf(temp, "File Name: %s\nError Code: %d\nLines read: %d\n", filename, error.code, error.lineto);
    return Py_BuildValue( "s", temp );
  }

  fclose( pyFile );

  PyList_Append( result, Py_BuildValue( "k", (unsigned long*)pcal ) );
  buildTupleList( pcal, result );

  return Py_BuildValue( "s", "OK" );
}


static PyObject *Cal_writeFile( PyObject *self, PyObject *args )
{
  CalStatus error;
  error.code = OK;
  error.linefrom = 0;
  error.lineto = 0;

  CalComp *pcal = NULL;

  char *filename = NULL;
  PyObject *compList = NULL;
  FILE *pyFile = NULL;

  PyArg_ParseTuple( args, "skO", &filename, (unsigned long*)&pcal, &compList );

  if( PyList_Size(compList) == 1 )
  {
    int i = 0;
    compList = PyList_GetItem(compList, 0);
    PyArg_Parse( compList, "i", &i );

    pyFile = fopen( filename, "w" );

    writeCalComp( NULL, NULL );
    error = writeCalComp( pyFile, pcal->comp[i] );
    fclose( pyFile );
    if( error.code != OK )
    {
      char temp[100];
      sprintf(temp, "File Name: %s\nError Code: %d\nLines written: %d\n", filename, error.code, error.lineto);
      return Py_BuildValue( "s", temp );
    }
  }
  else
  {
    if( PyList_Size(compList) < pcal->ncomps )
    {
      int i[150];
      int size = PyList_Size(compList);

      for( int j = 0; j < size; j ++)
      {
        PyArg_Parse( PyList_GetItem(compList, j), "i", &i[j] );
      }

      pyFile = fopen( filename, "w" );
      error = removeTodo( pcal, i, size, pyFile );
      fclose( pyFile );
      if( error.code != OK )
      {
        char temp[100];
        sprintf(temp, "File Name: %s\nError Code: %d\nLines written: %d\n", filename, error.code, error.lineto);
        return Py_BuildValue( "s", temp );
      }
    }
    else
    {
      pyFile = fopen( filename, "w" );

      writeCalComp( NULL, NULL );
      error = writeCalComp( pyFile, pcal );
      fclose( pyFile );
      if( error.code != OK )
      {
        char temp[100];
        sprintf(temp, "File Name: %s\nError Code: %d\nLines written: %d\n", filename, error.code, error.lineto);
        return Py_BuildValue( "s", temp );
      }
    }
  }

  char temp[100];
  sprintf(temp, "Status: %d\nLines written: %d\n", error.code, error.lineto);
  return Py_BuildValue( "s", temp );
}


static PyObject *Cal_freeFile( PyObject *self, PyObject *args )
{
  CalComp *pcal;
  PyArg_ParseTuple( args, "k", (unsigned long*)&pcal );

  freeCalComp( pcal );

  return Py_BuildValue( "s", "ok" );
}


static PyMethodDef CalMethods[] = {
    {"readFile", Cal_readFile, METH_VARARGS},
    {"writeFile", Cal_writeFile, METH_VARARGS},
    {"storeFile", Cal_storeFile, METH_VARARGS},
    {"freeFile", Cal_freeFile, METH_VARARGS},
    {NULL, NULL}
};


static struct PyModuleDef calModuleDef = {
    PyModuleDef_HEAD_INIT,
    "Cal", //enable "import Cal"
    NULL, //omit module documentation
    -1, //don't reinitialize the module
    CalMethods //link module name "Cal" to methods table };
};


PyMODINIT_FUNC PyInit_Cal(void) { return PyModule_Create( &calModuleDef );}
