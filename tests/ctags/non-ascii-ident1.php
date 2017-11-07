<?php
class × {
  private $r = 0;
  public function __construct($a, $b) {
    $this->r = $a * $b;
  }
  public function __toString() {
    return (string) $this->r;
  }
}

echo new ×(2, 2);
