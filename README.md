## Industrial Strength Pipes

Industrial Strength Pipes (ISP) is a toolkit for constructing pipeline
applications using the UNIX pipe and filter model.
In UNIX, the "do one thing and do it well" philosophy as applied to text
filters is considered a big success.  ISP tries to capitalize on this 
in applications that look architecturally similar to UNIX pipelines but
require more sophisticated interactions between filters.
Like UNIX filters, ISP filters can have their standard output and standard 
input chained with a shell command such as:
```
foo | bar | baz >result
```
Unlike UNIX filters which pass unstructured character streams or
newline-separated lines, ISP filters pass structured information in XML
format. The XML contains a stream of records that are operated on by
filters, analogous to the stream of lines processed by _grep_.

ISP is geared toward pipelines that apply a succession
of transformation algorithms to data files, producing and
consuming metadata and files associated with the original data along the way.
ISP was designed to be simple to use and program with a familiar
pipeline construction and execution model, and yet provide a framework that
allows for advanced features to be implemented
such as file management, strong metadata typing, parallel execution,
fault tolerance, data provenance, and logging.

Filters obtain ISP services through an application
programming interface (API) written in the C programming language. The
ISP package consists of the API headers and libraries and a
small collection of filters.

ISP is licensed under the terms of the GNU General Public License.

### Hello World

[hello.c](hello/hello.c)
is a filter that reads two integers _x_ and
_y_ out of each record, multplies them, and puts the result, _z_,
back into the record.

Presuming ISP is installed where the compiler can find its header
and library, the filter is compiled with the command:
```
cc -o hello hello.c -lisp -lexpat -lssl
```

The filter can be tested using `ispunit`, which generates one
record with content defined on its command line:
```
ispunit -i x=4 -i y=2 | ./hello | more
```

ISP's dynamic binding is demonstrated by omitting _x_ from ispunit:
```
$ ispunit -i y=2 | ./hello >/dev/null
hello[1]: requires key "x" (int64) not found upstream
hello[1]: isp_init: Pipeline binding error
```

For more information on ISP, see the
[documentation](doc/report.pdf)
