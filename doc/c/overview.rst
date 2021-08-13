############
Using Sratom
############

.. default-domain:: c
.. highlight:: c

The sratom API is declared in ``sratom.h``:

.. code-block:: c

   #include <sratom/sratom.h>

Sratom supports two operations:
writing atoms to a model or string, called "dumping",
and reading atoms from a model or string, called "loading".
Each is supported by a separate object.

*************
Dumping Atoms
*************

Dumping atoms is supported by :struct:`SratomDumper`.
A new dumper can be allocated with :func:`sratom_dumper_new`.
It requires a :struct:`SerdWorld`,
and both a URID map (to map vocabulary during setup),
and URID unmap (for expanding URIDs in the output).
For example, assuming these are already set up appropriately in the application:

.. code-block:: c

   SerdWorld*      world  = my_world();
   LV2_URID_Map*   map    = my_urid_map();
   LV2_URID_Unmap* unmap  = my_urid_map();
   SratomDumper*   dumper = sratom_dumper_new(world, map, unmap);

Dumping Strings
===============

Once a dumper is allocated it can be used to write any number of atoms.
The simplest function to use is :func:`sratom_to_string`,
which takes an environment and an atom,
and returns a newly allocated string representation of the atom.

The environment can be used to set up the base URI and any namespace prefixes for the output.
The base URI is used to convert relative paths to URIs,
and prefixes will be used to abbreviate the output.
Typically, the base URI is a file URI that points to a directory that file paths are relative to,
and prefixes are set for commonly used vocabularies.
For example:

.. code-block:: c

   SerdStringView base = SERD_STRING("file:///tmp/state/");
   SerdEnv*       env  = serd_env_new(base);

   serd_env_set_prefix(
     env,
     SERD_STRING("atom"),
     SERD_STRING("http://lv2plug.in/ns/ext/atom#"));

With the environment configured,
:func:`sratom_to_string` can be used to convert atoms to strings:

.. code-block:: c

   LV2_Atom* atom   = my_atom();
   char*     string = sratom_to_string(dumper, env, atom, 0);

   fprintf(stderr, "Atom: %s\n", string);

   sratom_free(string);

Dumping to a Statement Sink
===========================

More advanced use cases are supported by the more fundamental functions :func:`sratom_dump` and :func:`sratom_dump_atom`.
These write a series of statements that describe the atom to a :struct:`SerdSink`,
which can be configured to write anywhere, such as a file or model.
A subject and predicate should be provided for the main resulting statement.
For example, when writing a value for some control,
the subject might identify the device,
and the predicate the parameter:

.. code-block:: c

   SerdModel*      model     = my_model();
   LV2_URID        subject   = urid_map("http://example.org/amp");
   LV2_URID        predicate = urid_map("http://example.org/gain");
   const SerdSink* sink      = serd_inserter_new(model, NULL);

   const float value = 42.0f;

   sratom_dump(dumper,
               env,
               sink,
               subject,
               predicate,
               urid_map(LV2_ATOM__Float),
               sizeof(float),
               &value,
               0);

Which would produce output like:

.. code-block:: text

   eg:amp eg:gain 42.0 .

More complex atoms might produce several statements,
for example an object may itself have several properties:

.. code-block:: text

   eg:mixer eg:pan [
     eg:left 1.0 ;
     eg:front 0.5 ;
   ] .

Representation
==============

If no subject and predicate are given,
either explicitly with :func:`sratom_dump` or by using :func:`sratom_to_string`,
then the atom will be written as the subject.
Literals (which are not enough to form a statement) are written as the sole element of a list.
This ensures that the representation can be transmitted, stored, or transformed without loss.
For example, here is the terse string format (which is Turtle) of various atom types:

.. code-block:: turtle

   ( "hello" ) .

   ( true ) .

   ( 1 ) .

   ( 3.0 ) .

   ( <file:///absolute/path> ) .

   ( <relative/path> ) .

   ( eg:thing ) .

   []
       a atom:Tuple ;
       rdf:value ( "foo" true ) .

   []
       a atom:Vector ;
       atom:childType atom:Int ;
       rdf:value ( 1 2 3 4 5 ) .

   []
       a atom:Sequence ;
       rdf:value (
           [
               atom:frameTime 1 ;
               rdf:value "901A01"^^midi:MidiEvent
           ]
           [
               atom:frameTime 3 ;
               rdf:value "902B02"^^midi:MidiEvent
           ]
       ) .

Cleaning Up
===========

When finished, a dumper must be destroyed by :func:`sratom_dumper_free`:

.. code-block:: c

   sratom_dumper_free(dumper);

Any newly-allocated strings returned by :func:`sratom_to_string` are independent,
and may outlive the dumper that created them.
These must be individually destroyed with :func:`sratom_free`.

*************
Loading Atoms
*************

:struct:`SratomLoader` can construct atoms from descriptions written by a dumper.
This is typically used to load saved atoms from a file, socket, or data model.
A new loader can be allocated with :func:`sratom_loader_new`:

.. code-block:: c

   SerdWorld*    world  = my_world();
   LV2_URID_Map* map    = my_urid_map();
   SratomLoader* loader = sratom_loader_new(world, map);

Once a loader is allocated it can be used to read any number of atoms.

Loading Strings
===============

:func:`sratom_from_string` can be used to load atoms from strings created by :func:`sratom_to_string`:

.. code-block:: c

   const char* string = "( 42.0 ) .";

   LV2_Atom* atom = sratom_from_string(loader, env, string);

   do_something_with(atom);

The environment should match the one used when dumping the string,
so that namespace prefixes can be parsed correctly.
The returned atom is newly allocated and owned by the caller,
who must eventually destroy it with :func:`sratom_free`:

.. code-block:: c

   sratom_free(atom);

Loading from a Model
====================

:func:`sratom_from_model` and the lower-level :func:`sratom_load` can be used to load atoms from a data model.
A model contains statements,
so this can be used to load atoms that were saved with :func:`sratom_dump`.
The node that represents the atom must be given to specify where in the model to find the atom.
Typically,
this is the object of a statement with the subject and predicate passed to :func:`sratom_dump`.

For example,
given some model and node in an application,
a new atom can be allocated from its representation in the model:

.. code-block:: c

   const SerdModel* model      = my_model();
   const SerdNode*  value_node = get_value(model);

   Atom* atom = sratom_from_model(loader,
                                  base,
                                  model,
                                  value_node);

The lower-level :func:`sratom_load` can be used with a :struct:`LV2_Atom_Forge` instead,
which allows writing the atom directly to an existing buffer:

.. code-block:: c

   LV2_Atom_Forge* forge = my_buffer_writing_forge();

   sratom_load(loader,
               base,
               forge,
               model,
               value_node);

Cleaning Up
===========

When finished, a loader must be destroyed by :func:`sratom_loader_free`:

.. code-block:: c

   sratom_loader_free(loader);

Any newly-allocated atoms returned by :func:`sratom_from_model` are independent,
and may outlive the loader that created them.
These must be individually destroyed with :func:`sratom_free`.
