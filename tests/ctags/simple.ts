class CPoint {
  x: number;
  y: number;
  constructor(x: number, y: number) {
    this.x = x;
    this.y = y;
  }
}

class BankAccount {
  balance = 0;
  deposit(credit: number) {
    this.balance += credit;
    return this.balance;
  }
}

class CheckingAccount extends BankAccount {
  constructor(balance: number) {
    super(balance);
  }
  writeCheck(debit: number) {
    this.balance -= debit;
  }
}

class List<T extends NamedItem> {
  next: List<T> = null;
  constructor(public item: T) {
  }
  insertAfter(item: T) {
    var temp = this.next;
    this.next = new List(item);
    this.next.next = temp;
  }
  log() {
    console.log(this.item.name);
  }
  // ...
}

class C {
  x: number;
  static x: string;
}

class Messenger {
  message = "Hello World";
  start() {
    var _this = this;
    setTimeout(function() { alert(_this.message); }, 3000);
  }
};

class D {
  data: string | string[];
  getData() {
    var data = this.data;
    return typeof data === "string" ? data : data.join(" ");
  }
}

class Point {
  protected fakePointBuilder: () => { x: number, y: number };
  constructor(public x: number, public y: number) { }
  public length() { return Math.sqrt(this.x * this.x + this.y * this.y); }
  static origin = new Point(0, 0);
}

class A {
  private x: number;
  protected y: number;
  public fun: (a: 22 | 30, b: CPoint) => number | string;

  static f(a: A, b: B) {
    a.x = 1; // Ok
    b.x = 1; // Ok
    a.y = 1; // Ok
    b.y = 1; // Ok
  }

  getXAsT<T = any>(): T {
    return this.x as T;
  }

  register(...args) {
    return this.f(...args);
  }

  longArgsFun(options: {
    root: string;
    prefix?: string;
    setHeaders?: Function;
    send?: any;
  }) {
    return this.f(options);
  }

  closure(
    x: number,
  ): (path: string, callback: Function) => any {
    const normalizedPath = path === '/*' ? '' : path;
  }
}

namespace Validation {
    export interface StringValidator {
        isAcceptable(s: string): boolean;
    }

    const lettersRegexp = /^[A-Za-z]+$/;
    const numberRegexp = /^[0-9]+$/;

    export class LettersOnlyValidator implements StringValidator {
        isAcceptable(s: string) {
            return lettersRegexp.test(s);
        }
    }

    export class ZipCodeValidator implements StringValidator {
        isAcceptable(s: string) {
            return s.length === 5 && numberRegexp.test(s);
        }
    }
}

// Some samples to try
let strings = ["Hello", "98052", "101"];

// Validators to use
let validators: { [s: string]: Validation.StringValidator; } = {};
validators["ZIP code"] = new Validation.ZipCodeValidator();
validators["Letters only"] = new Validation.LettersOnlyValidator();

// Show whether each string passed each validator
for (let s of strings) {
    for (let name in validators) {
        console.log(`"${ s }" - ${ validators[name].isAcceptable(s) ? "matches" : "does not match" } ${ name }`);
    }
}

interface Mover {
  move(): void
  getStatus(): { speed: number }
}

interface Shaker {
  shake(): void
  getStatus(): { frequency: number }
}

interface MoverShaker extends Mover, Shaker {
  getStatus(/*)*/): { speed: number; frequency: number }
  //somethingCommentedOut(): string
  getSomething(): /* } */ void
  getSomethingSophisticated()/*
    comment
    block
    */: void

  getTpl<T>(): Promise<T>
}

interface SimpleRecord {
  propertyA: number
  propertyB: string
  readonly propertyC: []
  withoutType?
}

interface Document {
  createElement(tagName: "div"): HTMLDivElement
  createElement(tagName: "span"): HTMLSpanElement
  createElement(tagName: "canvas"): HTMLCanvasElement
  createElement(tagName: string): HTMLElement
}

interface CompilerOptions {
  strict?: boolean
  sourcePath?: string
  targetPath?: string
}

interface List<T> {
  data: T
  next: List<T>
  owner: List<List<T>>
}

interface JQuery {
  text(content: string)
}

interface JQueryStatic {
  get(url: string, callback: (data: string) => any)
  (query: string): JQuery
}

interface Array<T> {
  length: number
  [x: number]: T
  // Other members
}

enum Color { Red, Green, Blue }
enum Test {
  A,
  B,
  C = Math.floor(Math.random() * 1000),
  D = 10,
  E
}

export enum Style {
  None = 0,
  Bold = 1,
  Italic = 2,
  Underline = 4,
  Emphasis = Bold | Italic,
  Hyperlink = Bold | Underline
}

const enum Comparison {
  LessThan = -1,
  EqualTo = 0,
  GreaterThan = 1
}

function add(x, y) {
  return x + y;
}