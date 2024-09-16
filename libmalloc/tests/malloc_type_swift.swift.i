import Foundation

// sizeof(struct stat) == 144

#define DEFINE_TMO_SWIFT_TEST_CLASS(block, id1, id2) \
    class TestClass##id1##id2 { \
        let a: Int = 0; \
        let b: Int = 0; \
        let c: Int = 0; \
        let d: stat = stat(); \
        let e: stat = stat(); \
        let f: stat = stat(); \
        init() { \
            \
        } \
    };

#define TMO_SWIFT_TEST_CLASS_L1(action, block, id1) \
    action(block, id1, 0) \
    action(block, id1, 1) \
    action(block, id1, 2) \
    action(block, id1, 3) \
    action(block, id1, 4) \
    action(block, id1, 5) \
    action(block, id1, 6) \
    action(block, id1, 7) \
    action(block, id1, 8) \
    action(block, id1, 9)

#define TMO_SWIFT_TEST_CLASS_L2(action, block) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 0) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 1) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 2) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 3) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 4) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 5) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 6) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 7) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 8) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 9) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 10) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 11) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 12) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 13) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 14) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 15) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 16) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 17) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 18) \
    TMO_SWIFT_TEST_CLASS_L1(action, block, 19)

#define FOREACH_SWIFT_TMO_TEST_CLASS(action, block) \
    TMO_SWIFT_TEST_CLASS_L2(action, block)

FOREACH_SWIFT_TMO_TEST_CLASS(DEFINE_TMO_SWIFT_TEST_CLASS, "")

#define INVOKE_FOR_SWIFT_TMO_TEST_CLASS(block, id1, id2) \
    block(TestClass##id1##id2)

@_cdecl("test_swift_bucketing")
func test_swift_bucketing() {
    var objs: [AnyObject] = []

#define append_tmo_test_class(type) objs.append(type());
    FOREACH_SWIFT_TMO_TEST_CLASS(INVOKE_FOR_SWIFT_TMO_TEST_CLASS,
        append_tmo_test_class)

    validate_obj_array(objs)
}
