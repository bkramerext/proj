### 0. Introduction

This tutorial consists of 11 steps. Each shows an **Input** command and the **Output** it produces; run them from the `tutorial` directory to follow along.

**proj** works with XML and JSON as well as CSV, but this tutorial focuses on command syntax and query mechanics using the provided CSV files. Provided an XML element (or JSON property) name matches the field names used here, the same queries apply unchanged to XML or JSON versions of the same data.

<br>

- - -

### 1. Build and deploy proj (in the parent directory), if not already done.

Install a compiler per OS:

macOS: clang is expected to be preinstalled.

Ubuntu:
```
sudo apt-get update
sudo apt-get install clang libc++-dev
```

Windows: not yet documented.

Build and deploy:
Input:
```
make deploy
cd tutorial
```

<br>

- - -

### 2. Examine the field names, taken from the header row.
Input:
```
cat orders.csv | head -1
```
Output:
```
Row ID,OrderID,Order Date,Ship Date,Ship Mode,Customer ID,Customer Name,Segment,Country,City,State,Postal Code,Region,Product ID,Category,Sub-Category,Product Name,Sales,Quantity,Discount,Profit
```

<br>

- - -

### 3. Retrieve the first five orders' dates and customer names, reading from a file rather than standard input.
Input:
```
proj --in=orders.csv Order\ Date Customer\ Name first[5]
```
Output:
```
Order Date,Customer Name
1/4/13,Phillina Ober
1/4/13,Phillina Ober
1/4/13,Phillina Ober
1/5/13,Mick Brown
1/6/13,Lycoris Saunders
```
Explanation:<br> standard input and `--in` are equivalent, except in certain buffering cases where **proj** raises an error with standard input.

**proj** does not use the file extension to determine format. Instead, it attempts to parse the input as JSON, then XML, then a Log4j-style log file, then tab-separated (TSV), then comma-separated (CSV) as a last resort, at which point any file is accepted on a garbage-in, garbage-out basis. JSON embedded within log lines is expanded automatically.

`first[5]` is a *directive*: a specification that filters or shapes the output without producing its own column. It restricts **proj** to the first 5 input rows. This differs from `top[n]`, which applies its cutoff after filtering and sorting have occurred.

The escaped space allows the argument to match the input field `Customer Name`. This can also be written using curly braces and a quoted string: `{"Customer Name"}`. The brace form is required when a field name contains characters that would otherwise be interpreted as operators; see step 11.

<br>

- - -

### 4. Assign a custom header to a field, noting that matching is case-insensitive by default.
Input:
```
cat orders.csv | proj Date:order\ date Customer:customer\ name first[5]
```
Output:
```
Date,Customer
1/3/13,Darren Powers
1/4/13,Phillina Ober
1/4/13,Phillina Ober
1/4/13,Phillina Ober
1/5/13,Mick Brown
```
Explanation:<br> because `--case=true` (equivalently `case[true]`) was not specified, matching is case-insensitive throughout: field paths, column names, and function names alike.

**proj** derives a default column name from each column's expression. Prefixing an expression with `name:` overrides this default.

<br>

- - -

### 5. List, then count, the distinct customers.
Input:
```
proj --in=orders.csv name:customer\ name --distinct
```
Output:
```
name
Darren Powers
Phillina Ober
Mick Brown
... and 790 more rows
```
Explanation:<br> every column specification is an expression, with an alternative flag-style syntax available for convenience. Whether one writes `--distinct` or `distinct[]`, or `--first=5` or `first[5]`, is a matter of style.

There is no direct equivalent of SQL's `COUNT DISTINCT`. Instead, pipe the deduplicated result into a second **proj** invocation:

Input:
```
proj --in=orders.csv --distinct name:Customer\ Name | proj count[name]
```
Output:
```
count[name]
793
```

<br>

- - -

### 6. Sum profit by segment, with a custom header.
Input:
```
cat orders.csv | proj Segment \"Profit\ in\ \$1000\'s\":\"$\"\&round[sum[profit]/1000,2]\&\"K\"
```
Output:
```
Segment,Profit in $1000's
Consumer,$134.12K
Home Office,$60.3K
Corporate,$91.98K
```
Explanation:<br> **proj** provides a number of built-in functions (documentation forthcoming; until then, see `XmlOperatorFactory` in `xml_lib/xmlop.h` for the full list). This example uses `round[expr, num-dec-places]` and the infix string-concatenation operator `&`. A `concat[str1, str2]` function is also available and is equivalent to `str1 & str2`.

Most of the escaping above results from Bash tokenizing the command line before **proj** receives it. One way to avoid this is to move arguments into a file:

Simplified, with a descending `sort` added:
```
cat orders.csv | proj Segment @profitArg sort[-sum[profit]]
```
Output:
```
Segment,Profit in $1000's
Consumer,$134.12K
Corporate,$91.98K
Home Office,$60.3K
```
Explanation:<br> argument files — filenames prefixed or suffixed with `@` — allow arguments to be reused, keep commands readable, and avoid Bash's escaping requirements.

<br>

- - -

### 7. Find the top 10 customers by order count, sorted by descending order count and then by name.
```
cat orders.csv | proj Customer:Customer\ Name Orders:count[OrderID] sort[-Orders,Customer] top[10]
```
Output:
```
Customer,Orders
William Brown,37
Matt Abelman,34
John Lee,34
Paul Prost,34
Chloris Kastensmidt,32
Jonathan Doherty,32
Seth Vernon,32
Edward Hooks,32
Zuschuss Carroll,31
Arthur Prichep,31
```

Explanation:<br> `sort[]` accepts one or more sort keys, ordered from major to minor. Prefixing a key with `-` reverses its order; for string values this is accomplished by coercing to string and negating.

`count[]` is an aggregate function. Any column that is not an aggregate is treated as a group.

<br>

- - -

### 8. Show total profit by state, for the South region only.
Input:
```
cat orders.csv | proj State @profitArg where[region==\"South\"]
```
Output:
```
state,Profit in $1000's
Georgia,$16.25K
Kentucky,$11.2K
Virginia,$18.6K
Louisiana,$2.2K
South Carolina,$1.77K
Arkansas,$4.01K
Tennessee,$-5.34K
Florida,$-3.4K
North Carolina,$-7.49K
Mississippi,$3.17K
Alabama,$5.79K
```
Explanation:<br> string literals are quoted (escaped here because of Bash). The comparison against `region` matches case-insensitively against the data's `Region` column, since `--case=true` was not specified.

`where[pred-expr]` retains only rows for which `pred-expr` evaluates to true (non-zero). Multiple constraints can be expressed with the logical-AND operator `&&`, or with multiple `where` directives.

<br>

- - -

### 9. Find customers with exactly one order, and note why aggregates cannot be nested.
Input:
```
cat orders.csv | proj Name:customer\ name where[count[orderid]==1]
```
Output:
```
Name
Ricardo Emerson
Lela Donovan
Anthony O'Donnell
Carl Jackson
Jocasta Rupert
```
Discussion:<br>
To obtain a *count* of customers with exactly one order, an approach modeled on Excel's SUMIF might be:
```
cat orders.csv | proj Name:customer\ name sum[if[count[orderid]==1,1,0]]
```
This is not supported. **proj** returns:
```
Aggregate functions cannot be composed
```
Instead, pipe the filtered result into a second pass and count that:
```
cat orders.csv | proj Name:customer\ name where[count[orderid]==1] | proj count[Name]
```
Output:
```
count[Name]
5
```

<br>

- - -

### 10. Join the returns file to report on ten returned products and their reasons.
Input:
```
cat orders.csv | proj join[returns.csv] where[orderid==right::orderid] Product\ Name Reason:right::Reason top[10]
```
Output:
```
Product Name,Reason
"Wirebound Service Call Books, 5 1/2"" x 4""",Product Description Inaccurate
"Eldon Expressions Desk Accessory, Wood Pencil Holder, Oak",Product Description Inaccurate
Staple-on labels,Product Description Inaccurate
GBC Plastic Binding Combs,Product Description Inaccurate
"Acco Pressboard Covers with Storage Hooks, 9 1/2"" x 11"", Executive Red",Incorrect Products Delivered
"GBC Twin Loop Wire Binding Elements, 9/16"" Spine, Black",Incorrect Products Delivered
Xerox 1957,Incorrect Products Delivered
EcoTones Memo Sheets,Incorrect Products Delivered
Belkin 6 Outlet Metallic Surge Strip,Incorrect Products Delivered
"Bush Heritage Pine Collection 5-Shelf Bookcase, Albany Pine Finish, *Special Order",Customer Dissatified With Product
```

Explanation:<br> a join has three principal parts:
1. A `join[path]` directive naming the file to join against (XML, JSON, or CSV/TSV).
2. One or more `where[pred]` constraints relating the joined file's values to the main input's.
3. Scope prefixes distinguishing the two sides: `right::path` refers to the joined file.

Note that the output re-quotes literal quotation marks from the source data as doubled quotes, standard CSV escaping.

**proj** attempts to optimize joins by building index tables for use in `where` evaluation.

Only left joins are built in; for a right join, swap which file is treated as the main input and which is joined. An inner join is performed by default. An outer join is obtained by passing a second argument: `join[path, true]`.

<br>

- - -

### 11. Aggregate return reasons by category and sub-category, using the join again.

```
cat orders.csv | proj Returns:join[returns.csv] where[orderid==Returns::orderid] Category {Sub-Category} count[orderid] sort[Category] outheader[false]
```
Explanation:<br> results are grouped by both `Category` and `Sub-Category`. The braces around `{Sub-Category}` are required so the hyphen in the name is not interpreted as subtraction.

This example also renames the join's default `right::` scope to `Returns::`, by assigning the `join` directive its own column name.

The field passed to `count[]` — `orderid` here — is arbitrary; any field is generally suitable for a `count` aggregate. `count[Returns::Reason]` would serve equally well.

`outheader[false]` (equivalently `--outheader=false`) instructs **proj** to omit the CSV header row.

<br>

- - -
