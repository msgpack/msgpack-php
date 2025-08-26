<?php

class MessagePack {

    public const OPT_PHPONLY = -1001;
    public const OPT_ASSOC = -1002;

    public function __construct(bool $phponly = false) {}

    public function pack(mixed $data): string {}

    public function setOption(int $option, mixed $value): bool {}

    public function unpack(string $serialized): mixed {}

    public function unpacker(): MessagePackUnpacker {}
}

class MessagePackUnpacker {

    public function __construct(bool $phponly = false) {}

    public function __destruct() {}

    public function data(string|object|null $object = null): mixed {}

    public function execute(int|float|null $offset = null): bool {}

    public function feed(string $chunk): bool {}

    public function reset(): void {}

    public function setOption(int $option, mixed $value): bool {}
}

function msgpack_serialize(mixed $value): string {}

/**
 * alias for msgpack_serialize()
 */
function msgpack_pack(mixed $value): string {}

function msgpack_unserialize(string $str): mixed {}

/**
 * alias for msgpack_unserialize()
 */
function msgpack_unpack(string $str): mixed {}
