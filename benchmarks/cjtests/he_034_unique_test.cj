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
    @Expect(typed_fn_solve(listOf([5, 3, 5, 2, 3, 3, 9, 0, 123])), listOf([0, 2, 3, 5, 9, 123]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([])), listOf([]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([4])), listOf([4]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([1, 1, 1])), listOf([1]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([-2, 3, -2])), listOf([-2, 3]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([9, 8, 7])), listOf([7, 8, 9]))
}
