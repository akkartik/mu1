//: Instructions can read from addresses pointing at other locations using the
//: 'lookup' property.

:(scenario copy_indirect)
def main [
  1:address:number <- copy 2/unsafe
  2:number <- copy 34
  # This loads location 1 as an address and looks up *that* location.
  3:number <- copy 1:address:number/lookup
]
+mem: storing 34 in location 3

:(before "End Preprocess read_memory(x)")
canonize(x);

//: similarly, write to addresses pointing at other locations using the
//: 'lookup' property
:(scenario store_indirect)
def main [
  1:address:number <- copy 2/unsafe
  1:address:number/lookup <- copy 34
]
+mem: storing 34 in location 2

:(before "End Preprocess write_memory(x)")
canonize(x);
if (x.value == 0) {
  raise << "can't write to location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
  return;
}

//: writes to address 0 always loudly fail
:(scenario store_to_0_fails)
% Hide_errors = true;
def main [
  1:address:number <- copy 0
  1:address:number/lookup <- copy 34
]
-mem: storing 34 in location 0
+error: can't write to location 0 in '1:address:number/lookup <- copy 34'

:(code)
void canonize(reagent& x) {
  if (is_literal(x)) return;
  // End canonize(x) Special-cases
  while (has_property(x, "lookup"))
    lookup_memory(x);
}

void lookup_memory(reagent& x) {
  if (!x.type || x.type->value != get(Type_ordinal, "address")) {
    raise << maybe(current_recipe_name()) << "tried to /lookup " << x.original_string << " but it isn't an address\n" << end();
    return;
  }
  // compute value
  if (x.value == 0) {
    raise << maybe(current_recipe_name()) << "tried to /lookup 0\n" << end();
    return;
  }
  trace(9999, "mem") << "location " << x.value << " is " << no_scientific(get_or_insert(Memory, x.value)) << end();
  x.set_value(get_or_insert(Memory, x.value));
  drop_from_type(x, "address");
  // End Drop Address In lookup_memory(x)
  drop_one_lookup(x);
}

:(after "bool types_strictly_match(reagent to, reagent from)")
  if (!canonize_type(to)) return false;
  if (!canonize_type(from)) return false;

:(after "bool is_mu_array(reagent r)")
  if (!canonize_type(r)) return false;

:(after "bool is_mu_address(reagent r)")
  if (!canonize_type(r)) return false;

:(after "bool is_mu_number(reagent r)")
  if (!canonize_type(r)) return false;
:(after "bool is_mu_boolean(reagent r)")
  if (!canonize_type(r)) return false;

:(code)
bool canonize_type(reagent& r) {
  while (has_property(r, "lookup")) {
    if (!r.type || r.type->value != get(Type_ordinal, "address")) {
      raise << "can't lookup non-address: " << to_string(r) << ": " << to_string(r.type) << '\n' << end();
      return false;
    }
    drop_from_type(r, "address");
    // End Drop Address In canonize_type(r)
    drop_one_lookup(r);
  }
  return true;
}

void drop_from_type(reagent& r, string expected_type) {
  if (r.type->name != expected_type) {
    raise << "can't drop2 " << expected_type << " from " << to_string(r) << '\n' << end();
    return;
  }
  type_tree* tmp = r.type;
  r.type = tmp->right;
  tmp->right = NULL;
  delete tmp;
}

void drop_one_lookup(reagent& r) {
  for (vector<pair<string, string_tree*> >::iterator p = r.properties.begin(); p != r.properties.end(); ++p) {
    if (p->first == "lookup") {
      r.properties.erase(p);
      return;
    }
  }
  assert(false);
}

//:: abbreviation for '/lookup': a prefix '*'

:(scenario lookup_abbreviation)
def main [
  1:address:number <- copy 2/unsafe
  2:number <- copy 34
  3:number <- copy *1:address:number
]
+parse: ingredient: {1: ("address" "number"), "lookup": ()}
+mem: storing 34 in location 3

:(before "End Parsing reagent")
{
  while (!name.empty() && name.at(0) == '*') {
    name.erase(0, 1);
    properties.push_back(pair<string, string_tree*>("lookup", NULL));
  }
  if (name.empty())
    raise << "illegal name " << original_string << '\n' << end();
}

//:: helpers for debugging

:(before "End Primitive Recipe Declarations")
_DUMP,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$dump", _DUMP);
:(before "End Primitive Recipe Implementations")
case _DUMP: {
  reagent after_canonize = current_instruction().ingredients.at(0);
  canonize(after_canonize);
  cerr << maybe(current_recipe_name()) << current_instruction().ingredients.at(0).name << ' ' << no_scientific(current_instruction().ingredients.at(0).value) << " => " << no_scientific(after_canonize.value) << " => " << no_scientific(get_or_insert(Memory, after_canonize.value)) << '\n';
  break;
}

//: grab an address, and then dump its value at intervals
//: useful for tracking down memory corruption (writing to an out-of-bounds address)
:(before "End Globals")
int Bar = -1;
:(before "End Primitive Recipe Declarations")
_BAR,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$bar", _BAR);
:(before "End Primitive Recipe Implementations")
case _BAR: {
  if (current_instruction().ingredients.empty()) {
    if (Bar != -1) cerr << Bar << ": " << no_scientific(get_or_insert(Memory, Bar)) << '\n';
    else cerr << '\n';
  }
  else {
    reagent tmp = current_instruction().ingredients.at(0);
    canonize(tmp);
    Bar = tmp.value;
  }
  break;
}