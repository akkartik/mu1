//: Construct types out of their constituent fields.

void test_merge() {
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 3, 4\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 3 in location 1\n"
      "mem: storing 4 in location 2\n"
  );
}

:(before "End Primitive Recipe Declarations")
MERGE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "merge", MERGE);
:(before "End Primitive Recipe Checks")
case MERGE: {
  // type-checking in a separate transform below
  break;
}
:(before "End Primitive Recipe Implementations")
case MERGE: {
  products.resize(1);
  for (int i = 0;  i < SIZE(ingredients);  ++i)
    for (int j = 0;  j < SIZE(ingredients.at(i));  ++j)
      products.at(0).push_back(ingredients.at(i).at(j));
  break;
}

//: type-check 'merge' to avoid interpreting numbers as addresses

:(code)
void test_merge_check() {
  run(
      "def main [\n"
      "  1:point <- merge 3, 4\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_missing_element() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:point <- merge 3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too few ingredients in '1:point <- merge 3'\n"
  );
}

void test_merge_check_extra_element() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:point <- merge 3, 4, 5\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too many ingredients in '1:point <- merge 3, 4, 5'\n"
  );
}

//: We want to avoid causing memory corruption, but other than that we want to
//: be flexible in how we construct containers of containers. It should be
//: equally easy to define a container out of primitives or intermediate
//: container fields.

void test_merge_check_recursive_containers() {
  run(
      "def main [\n"
      "  1:point <- merge 3, 4\n"
      "  1:point-number <- merge 1:point, 5\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_recursive_containers_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:point <- merge 3, 4\n"
      "  2:point-number <- merge 1:point\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too few ingredients in '2:point-number <- merge 1:point'\n"
  );
}

void test_merge_check_recursive_containers_3() {
  run(
      "def main [\n"
      "  1:point-number <- merge 3, 4, 5\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_recursive_containers_4() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:point-number <- merge 3, 4\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too few ingredients in '1:point-number <- merge 3, 4'\n"
  );
}

void test_merge_check_reflexive() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:point <- merge 3, 4\n"
      "  2:point <- merge 1:point\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

//: Since a container can be merged in several ways, we need to be able to
//: backtrack through different possibilities. Later we'll allow creating
//: exclusive containers which contain just one of rather than all of their
//: elements. That will also require backtracking capabilities. Here's the
//: state we need to maintain for backtracking:

:(before "End Types")
struct merge_check_point {
  reagent container;
  int container_element_index;
  merge_check_point(const reagent& c, int i) :container(c), container_element_index(i) {}
};

struct merge_check_state {
  stack<merge_check_point> data;
};

:(before "End Checks")
Transform.push_back(check_merge_calls);  // idempotent
:(code)
void check_merge_calls(const recipe_ordinal r) {
  const recipe& caller = get(Recipe, r);
  trace(101, "transform") << "--- type-check merge instructions in recipe " << caller.name << end();
  for (int i = 0;  i < SIZE(caller.steps);  ++i) {
    const instruction& inst = caller.steps.at(i);
    if (inst.name != "merge") continue;
    if (SIZE(inst.products) != 1) {
      raise << maybe(caller.name) << "'merge' should yield a single product in '" << to_original_string(inst) << "'\n" << end();
      continue;
    }
    reagent/*copy*/ product = inst.products.at(0);
    // Update product While Type-checking Merge
    const type_tree* product_base_type = product.type->atom ? product.type : product.type->left;
    assert(product_base_type->atom);
    if (product_base_type->value == 0 || !contains_key(Type, product_base_type->value)) {
      raise << maybe(caller.name) << "'merge' should yield a container in '" << to_original_string(inst) << "'\n" << end();
      continue;
    }
    const type_info& info = get(Type, product_base_type->value);
    if (info.kind != CONTAINER && info.kind != EXCLUSIVE_CONTAINER) {
      raise << maybe(caller.name) << "'merge' should yield a container in '" << to_original_string(inst) << "'\n" << end();
      continue;
    }
    check_merge_call(inst.ingredients, product, caller, inst);
  }
}

void check_merge_call(const vector<reagent>& ingredients, const reagent& product, const recipe& caller, const instruction& inst) {
  int ingredient_index = 0;
  merge_check_state state;
  state.data.push(merge_check_point(product, 0));
  while (true) {
    assert(!state.data.empty());
    trace(102, "transform") << ingredient_index << " vs " << SIZE(ingredients) << end();
    if (ingredient_index >= SIZE(ingredients)) {
      raise << maybe(caller.name) << "too few ingredients in '" << to_original_string(inst) << "'\n" << end();
      return;
    }
    reagent& container = state.data.top().container;
    if (!container.type) return;  // error handled elsewhere
    const type_tree* top_root_type = container.type->atom ? container.type : container.type->left;
    assert(top_root_type->atom);
    type_info& container_info = get(Type, top_root_type->value);
    switch (container_info.kind) {
      case CONTAINER: {
        // degenerate case: merge with the same type always succeeds
        if (state.data.top().container_element_index == 0 && types_coercible(container, inst.ingredients.at(ingredient_index)))
          return;
        const reagent& expected_ingredient = element_type(container.type, state.data.top().container_element_index);
        trace(102, "transform") << "checking container " << to_string(container) << " || " << to_string(expected_ingredient) << " vs ingredient " << ingredient_index << end();
        // if the current element is the ingredient we expect, move on to the next element/ingredient
        if (types_coercible(expected_ingredient, ingredients.at(ingredient_index))) {
          ++ingredient_index;
          ++state.data.top().container_element_index;
          while (state.data.top().container_element_index >= SIZE(get(Type, get_base_type(state.data.top().container.type)->value).elements)) {
            state.data.pop();
            if (state.data.empty()) {
              if (ingredient_index < SIZE(ingredients))
                raise << maybe(caller.name) << "too many ingredients in '" << to_original_string(inst) << "'\n" << end();
              return;
            }
            ++state.data.top().container_element_index;
          }
        }
        // if not, maybe it's a field of the current element
        else {
          // no change to ingredient_index
          state.data.push(merge_check_point(expected_ingredient, 0));
        }
        break;
      }
      // End check_merge_call Special-cases
      default: {
        if (!types_coercible(container, ingredients.at(ingredient_index))) {
          raise << maybe(caller.name) << "incorrect type of ingredient " << ingredient_index << " in '" << to_original_string(inst) << "'\n" << end();
          raise << "  (expected '" << debug_string(container) << "')\n" << end();
          raise << "  (got '" << debug_string(ingredients.at(ingredient_index)) << "')\n" << end();
          return;
        }
        ++ingredient_index;
        // ++state.data.top().container_element_index;  // unnecessary, but wouldn't do any harm
        do {
          state.data.pop();
          if (state.data.empty()) {
            if (ingredient_index < SIZE(ingredients))
              raise << maybe(caller.name) << "too many ingredients in '" << to_original_string(inst) << "'\n" << end();
            return;
          }
          ++state.data.top().container_element_index;
        } while (state.data.top().container_element_index >= SIZE(get(Type, get_base_type(state.data.top().container.type)->value).elements));
      }
    }
  }
  // never gets here
  assert(false);
}

//: replaced in a later layer
//: todo: find some clean way to take this call completely out of this layer
const type_tree* get_base_type(const type_tree* t) {
  return t;
}

void test_merge_check_product() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:num <- merge 3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'merge' should yield a container in '1:num <- merge 3'\n"
  );
}

:(before "End Includes")
#include <stack>
using std::stack;
