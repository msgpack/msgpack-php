--TEST--
Bug #011 (Check for segfault with empty array structures)
--FILE--
<?php
	
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
