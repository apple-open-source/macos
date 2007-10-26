/* APPLE LOCAL file radar 4649701 */
/* Test that test compiles with no error when setter is declared in protocol, 
   not declared in its adopting interface and is defined in @implementation. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@class GroupItem;

@protocol GroupEditorProtocol
  - (void)setGroupItem:(GroupItem *)aGroupItem;
@end


@interface GroupEditor  <GroupEditorProtocol>
{
  GroupItem *groupItem;
}
@property (ivar=groupItem) GroupItem *groupItem_property;

@end

@implementation GroupEditor
- (void)setGroupItem:(GroupItem *)aGroupItem
{
}

@property (ivar=groupItem,setter=setGroupItem:) GroupItem *groupItem_property;
@end

