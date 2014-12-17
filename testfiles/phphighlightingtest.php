<?php
class MyClass {
	var $test = array();

	mysql_connect("mydb", 'user', 'pw');

	function MyClass($inputvar) {
		$this->test[] = $inputvar;
		$res = mysql_query("SELECT * FROM SOMETABLE WHERE KEY='1'");
		while ($row = mysql_fetch_row($res)) {
			echo $row[0];

		}
	}
	/* this is inside php code */
}
$var='?>';
?>
hallo
<IMG SRC="<?php echo $URL[1] ?>" BORDER="0">
a test &nspb; and so on " quote ` backtick

// this comment is outside the php-code so it is in fact not a comment
/* and this one contains some some quotes ' here and there " */
hmm we need some unicode characters..ÆÆæåùê®«¼½¾¿ßÿð¥

and <b>bold</b> and <I>italics</I>
<script type="text/javascript" >
// a javascript comment
var foo = "bar";

</script>
<?php
function play() {
	$test[0] = 'test for \' all kind of { highlighting ] things, and another test';
	return "a double quoted string' \"with braces\n {} ".$test[0];
}
 # and some other comment // bla */

 // in the php with a fake $var in /* there */ and a keyword: function
echo 'and some braces ( ] ) in a \t \' php string';
echo play();
$obj[] = new MyClass('test');
$i=1;
echo $obj[0]->test[$i];
// this is an assoc. array
$arr['mykey'] = 'myval';
/* and a different type comment
 * over 2 lines
 */
?>

<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>"/> 

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">
<?php
	echo "and another test<ul>";
	$res = mysql_query("select bla from bloep");
	while ($row =  mysql_fetch_row($res)) {
		echo "<li>".$row[0]."</li>";
	}
	echo "<ul>";

?>
<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">
<?php
	echo "this is a test!";
	$res = mysql_query("select * from some_table where a='0'");
	while ($row = mysql_fetch_row($res)) {
		var_dump($row);
	}


?>
<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">

<table border="0">
<tr><td align="center">a <a href="mypage.php">link</a> in a cell</td></tr>
</table>
<!-- <p>and another inside a comment</p> -->
<p align="center"> and a paragraph </p>
<!-- html comment -->
<img src="<?php echo $URL[2] ?>">


</body>
</html>
