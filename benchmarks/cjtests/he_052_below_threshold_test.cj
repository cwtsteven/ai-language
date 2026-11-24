import std.collection.ArrayList
import std.unittest.*
import std.unittest.testmacro.*

func listOf(items: Array<Int64>): ArrayList<Int64> {
    let out = ArrayList<Int64>()
    for (item in items) {
        out.add(item)
    }
    return out
}

@Test
func case0() {
    @Expect(typed_fn_solve(listOf([1, 2, 4, 10]), 100), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 20, 4, 10]), 5), false)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([1, 20, 4, 10]), 21), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([1, 20, 4, 10]), 22), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([1, 8, 4, 10]), 11), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([1, 8, 4, 10]), 10), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([]), 1), true)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([0]), 1), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve(listOf([5]), 5), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve(listOf([-5, 3]), 4), true)
}
