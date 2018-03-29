--TEST--
Bug #011 (Check for segfault with empty array structures)
--FILE--
<?php
if(!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

$items = array( );
foreach( range( 0, 1024 ) as $r ) {
	$items[] = array(
		'foo' => array( )
	);
}
var_dump( count( msgpack_unpack( msgpack_pack( $items ) ) ) );
	
?>
--EXPECT--
int(1025)
