--TEST--
Object reference keys must not collide between objects of different classes
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    exit('skip because msgpack extension is missing');
}
?>
--INI--
memory_limit=1G
--FILE--
<?php
class Item {
    public $status;
}

class Status {
}

$status = new Status();
$items = array();
for ($i = 0; $i < 200000; $i++) {
    $item = new Item();
    $item->status = $status;
    $items[] = $item;
}

$unpacked = msgpack_unpack(msgpack_pack($items));
foreach ($unpacked as $idx => $value) {
    if (!$value instanceof Item) {
        echo "index $idx: expected Item, got " . get_class($value) . "\n";
    }
}

// temporary objects created during packing must not be confused with later ones

class Tmp {
    public $v;
    public function __construct($v) { $this->v = $v; }
    public function __serialize(): array { return array(new Tmp2($this->v)); }
    public function __unserialize(array $data): void { $this->v = $data[0]->v; }
}

class Tmp2 {
    public $v;
    public function __construct($v) { $this->v = $v; }
}

$list = array();
for ($i = 0; $i < 100; $i++) {
    $list[] = new Tmp($i);
}
$unpacked = msgpack_unpack(msgpack_pack($list));
foreach ($unpacked as $idx => $value) {
    if (!$value instanceof Tmp || $value->v !== $idx) {
        echo "index $idx: wrong value\n";
    }
}
echo "done\n";
?>
--EXPECT--
done
