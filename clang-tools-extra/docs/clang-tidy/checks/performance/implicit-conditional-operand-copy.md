```{title} clang-tidy - performance-implicit-conditional-operand-copy
```

# performance-implicit-conditional-operand-copy

Finds conditional operators that implicitly copy one of their operands because
the other operand is a temporary, in contexts where the copy could be avoided.

When the second and third operands of a conditional operator are an lvalue and
a prvalue of the same class type, the result of the operator is a prvalue: the
lvalue operand is copied to produce it. The copy is easy to miss, because
nothing in the source spells it out.

```cpp
struct Expensive { /* ... */ };

Expensive makeExpensive();
void consume(const Expensive &);

void example(bool Cond, const Expensive &E) {
  // Warning: `E` is copied to produce the prvalue result of the conditional
  // operator, and that copy is then bound to the `const Expensive &`
  // parameter of `consume`.
  consume(Cond ? E : makeExpensive());
}
```

The copy can be avoided by rewriting the conditional operator as an `if`-`else`
statement:

```cpp
if (Cond)
  consume(E);
else
  consume(makeExpensive());
```

or by casting both operands to a reference type, which makes the conditional
operator itself an lvalue:

```cpp
consume(Cond ? static_cast<const Expensive &>(E)
             : static_cast<const Expensive &>(makeExpensive()));
```

Beware that the second form does not extend the lifetime of the temporary
returned by `makeExpensive()`, so it is only correct as long as the result is
consumed within the same full-expression. Because neither rewrite is always
appropriate, the check does not suggest a fix-it.

The check warns where dropping the copy is decidable without performing overload
resolution. That covers the temporary being used solely as a `const` object --
bound to a reference to const, or the object of a `const` member call that is
not `&&`-qualified and hands out no mutable access -- and it covers the value
being passed straight on to one known type, as in `Target = Cond ? A : make()`
or `return Cond ? A : make()`, when that type has an accessible, non-deleted,
non-template member taking the operand by value or by reference to const.

This relies on `const` meaning logically const. A `const` member function that
mutates `mutable` state, or returns a proxy through which the object can be
modified, can still make the rewrite observable, and no check can see that from
the call site.

Every other use would need overload resolution to decide, and a clang-tidy check
cannot perform it: the check runs on a finished AST, after `Sema` has been torn
down. Rather than guess, the check stays quiet there, so an implicit copy that
is mutated, or bound to an rvalue reference -- including a forwarding reference
deduced from the temporary, as `emplace_back` and `make_shared` produce -- is
not reported even when it happens to be removable.

What decides this is how the operand is used, not what is called. An argument
that binds `const T &` is reported whether or not the callee is a template.

```cpp
void takeOwnership(Expensive);
void sink(Expensive &&);

// Assignable from an lvalue, so `Assignable = E;` compiles.
struct Assignable {
  Assignable &operator=(const Expensive &);
  Assignable &operator=(Expensive &&);
};

// Assignable only from an rvalue, so `RValueOnly = E;` does not.
struct RValueOnly {
  RValueOnly &operator=(Expensive &&);
};

void positives(bool Cond, const Expensive &E, Assignable &A) {
  consume(Cond ? E : makeExpensive());              // warning
  const Expensive &R = Cond ? E : makeExpensive();  // warning
  (Cond ? E : makeExpensive()).constMethod();       // warning
  A = Cond ? E : makeExpensive();                   // warning
}

void negatives(bool Cond, Expensive &E, const Expensive &F, RValueOnly &R) {
  takeOwnership(Cond ? E : makeExpensive());   // a copy is needed anyway
  Expensive Y = Cond ? E : makeExpensive();    // a copy is needed anyway
  consume(Cond ? E : F);                       // both operands are lvalues, so
                                               // the result is an lvalue too

  // Not reported: whether the copy could be dropped depends on overload
  // resolution.
  sink(Cond ? E : makeExpensive());
  (Cond ? E : makeExpensive()).mutate();
  R = Cond ? E : makeExpensive();
}
```

Operands that are moved rather than copied are not reported, and neither are
types that are cheap to copy, that is, types that are trivially copyable and
trivially destructible.
