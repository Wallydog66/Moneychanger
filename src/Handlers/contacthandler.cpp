

#include "contacthandler.h"

#include "DBHandler.h"


//static
MTContactHandler * MTContactHandler::_instance = NULL;

//protected
MTContactHandler::MTContactHandler()
{
}


//static
MTContactHandler * MTContactHandler::getInstance()
{
    if (NULL == _instance)
    {
        _instance = new MTContactHandler;
    }
    return _instance;
}


//resume

// TODO:
// Function to delete a contact (I think also: leaving behind the nym/account/server records for that contact...?
// Otherwise cleaning those out. Either way, still needs to wipe the nym table's record of the contact ID that got wiped.)
// Function to return a list of servers, filtered by Nym.
// Function to return a list of accounts, filtered by Nym.
// Function to return a list of accounts, filtered by Server.
// Function to return a list of accounts, filtered by Server and Asset Type.
// Function to return a list of accounts, filtered by Nym and Server.
// Function to return a list of accounts, filtered by Nym and Asset Type.
// Function to return a list of accounts, filtered by Nym, Server, and Asset Type.
// Function to return a list of asset types, filtered by Nym (account join).
// Function to return a list of asset types, filtered by Server (account join).


// Notice there is no "CreateContactBasedOnAcct" because you can call this first,
// and then just call FindContactIDByAcctID.
//
int MTContactHandler::CreateContactBasedOnNym(QString nym_id_string, QString server_id_string/*=QString("")*/)
{
    QMutexLocker locker(&m_Mutex);

    // First, see if a contact already exists for this Nym, and if so,
    // save its ID and return at the bottom.
    //
    // If no contact exists for this Nym, then create the contact and Nym.
    // (And save the contact ID, and return at the bottom.)
    //
    // Finally, do the same actions found in NotifyOfNymServerPair, only if server_id_string
    // !isEmpty(), to make sure we record the server as well, when appropriate.
    //
    // -----------------------------------------------------------------------
    int nContactID = 0;

    // First, see if a contact already exists for this Nym, and if so,
    // save its ID and return at the bottom.
    //

    QString str_select = QString("SELECT `contact_id` FROM `nym` WHERE nym_id=%1").arg(nym_id_string);

    int  nRows      = DBHandler::getInstance()->querySize(str_select);
    bool bNymExists = false;

    for(int ii=0; ii < nRows; ii++)
    {
        nContactID = DBHandler::getInstance()->queryInt(str_select, 0, ii);

        bNymExists = true; // Whether the contact ID was good or not, the Nym itself DOES exist.

        break; // In practice there should only be one row.
    }
    // ---------------------------------------------------------------------
    // If no contact exists for this Nym, then create the contact and Nym.
    // (And save the contact ID, and return at the bottom.)
    //
    bool bHadToCreateContact = false;
    if (!(nContactID > 0))
    {
        bHadToCreateContact = true;
        QString str_insert_contact = QString("INSERT INTO `contact` "
                                             "(`contact_id`) "
                                             "VALUES(NULL)");
        DBHandler::getInstance()->runQuery(str_insert_contact);
        // ----------------------------------------
        nContactID = DBHandler::getInstance()->queryInt("SELECT last_insert_rowid() from `contact`", 0, 0);
    }
    // ---------------------------------------------------------------------
    // Here we create or update the Nym...
    //
    if (nContactID > 0)
    {
        QString str_insert_nym;

        if (!bNymExists)
            str_insert_nym = QString("INSERT INTO `nym` "
                                     "(`nym_id`, `contact_id`) "
                                     "VALUES('%1', '%2')").arg(nym_id_string).arg(nContactID);
        else if (bHadToCreateContact)
            str_insert_nym = QString("UPDATE nym SET `contact_id`='%1' WHERE nym_id=%2").arg(nContactID).arg(nym_id_string);

        DBHandler::getInstance()->runQuery(str_insert_nym);
    }
    // ---------------------------------------------------------------------
    // Finally, do the same actions found in NotifyOfNymServerPair, only if server_id_string
    // !isEmpty(), to make sure we record the server as well, when appropriate.
    //
    if (!server_id_string.isEmpty())
    {
        QString str_select_server = QString("SELECT `server_id` FROM `nym_server` WHERE `nym_id`=%1 AND `server_id`=%2 LIMIT 0,1").arg(nym_id_string).arg(server_id_string);
        int nRowsServer = DBHandler::getInstance()->querySize(str_select_server);

        if (0 == nRowsServer) // It wasn't already there. (Add it.)
        {
            QString str_insert_server = QString("INSERT INTO `nym_server` "
                                                "(`nym_id`, `server_id`) "
                                                "VALUES('%1', '%2')").arg(nym_id_string).arg(server_id_string);
            DBHandler::getInstance()->runQuery(str_insert_server);
        }
    }
    // ---------------------------------------------------------------------
    return nContactID;
}

/*
int MTContactHandler::FindOrCreateContactByNym(QString nym_id_string)
{
    int nContactID = this->FindContactIDByNymID(nym_id_string);

    if (nContactID > 0)
        return nContactID; // It already exists.
    // ----------------------------------------------
    "INSERT INTO `address_book` (`id`, `nym_id`, `nym_display_name`) VALUES(NULL, '%1', '%2')"
}
// NOTE: above might be unnecessary. See below comment.
*/

// Either you create a new Contact, or you use a pre-existing Contact.
//
// If you choose to add to a pre-existing Contact, then you have already
// selected him from a list, and you have his contact ID already. IN THAT
// CASE, you know the ContactID, and you have either a Nym, or a Nym/Server
// pair, or an Account (including nym, server, and asset) to add to that
// contact.
//
// Whereas if you choose to create a new Contact, you must choose to enter
// his name and then you get his new contact ID (the same as if you had
// selected him from a list) and then you can do the above, as normal.
//
// Therefore either way, we will know the Contact ID in advance, if we are
// DEFINITELY adding the data. And if we do NOT know the contact ID, that
// means we ONLY want to add the data in the case where it is found indirectly.
// (Otherwise we don't want to add it.)
//
// To some degree, we can figure out certain things automatically. For example,
// Let's say I have a receipt with some new account number on it. I might not
// have any record of that account in my contacts list. HOWEVER, the account
// might be owned by a Nym who IS in my contacts list, in which case I should
// be smart enough to add that account to the contact for that same Nym.
//
// Let's say the account isn't found in there at all, even indirectly. The function
// would then return a failure, which should be signal enough to the caller,
// to ask the user if he wants to add it to his address book. But that's not our
// place to do -- the caller will add it to his address book if he wishes. Until
// then we either just return a failure, or if we can find it indirectly, we add
// it to our records.


QString MTContactHandler::GetContactName(int nContactID)
{
    QMutexLocker locker(&m_Mutex);

    QString str_select = QString("SELECT `contact_display_name` FROM `contact` WHERE contact_id=%1 LIMIT 0,1").arg(nContactID);

    int nRows = DBHandler::getInstance()->querySize(str_select);

    for(int ii=0; ii < nRows; ii++)
    {
        //Extract data
        QString contact_name = DBHandler::getInstance()->queryString(str_select, 0, ii);

        return contact_name; // In practice there should only be one row.
    }

    return ""; // Didn't find anyone.
}

bool MTContactHandler::SetContactName(int nContactID, QString contact_name_string)
{
    QMutexLocker locker(&m_Mutex);

    QString str_update = QString("UPDATE contact SET `contact_display_name`='%1' WHERE Id=%2").arg(contact_name_string).arg(nContactID);

    return DBHandler::getInstance()->runQuery(str_update);
}



// If we have a specific Nym/Server pair, we check (using this function) to see
// if this server is already in my internal list for a given NymID. Notice this
// happens whether there is a CONTACT for that Nym, or not.
// If the pairing is not found, a record is added.
//
void MTContactHandler::NotifyOfNymServerPair(QString nym_id_string, QString server_id_string)
{
    QMutexLocker locker(&m_Mutex);

    QString str_select_server = QString("SELECT `server_id` FROM `nym_server` WHERE `nym_id`=%1 AND `server_id`=%2 LIMIT 0,1").
            arg(nym_id_string).arg(server_id_string);
    int nRowsServer = DBHandler::getInstance()->querySize(str_select_server);

    if (0 == nRowsServer) // It wasn't already there. (Add it.)
    {
        QString str_insert_server = QString("INSERT INTO `nym_server` "
                                            "(`nym_id`, `server_id`) "
                                            "VALUES('%1', '%2')").arg(nym_id_string).arg(server_id_string);
        DBHandler::getInstance()->runQuery(str_insert_server);
    }
}


// NOTE: if an account isn't ALREADY found in my contact list, I am
// very unlikely to find it in my wallet (since it's still most likely
// someone else's account) and so I am not able to look up its associated
// nymId, serverId, and assetTypeIds. Therefore if I have those available
// when I call this function, then I NEED to pass them in, so that if the
// account CAN be added to some existing contact, we will have its IDs to
// add.
// Therefore this function will not bother adding an account when the Nym
// is unknown.
//
int MTContactHandler::FindContactIDByAcctID(QString acct_id_string,
                                            QString nym_id_string/*=QString("")*/,
                                            QString server_id_string/*=QString("")*/,
                                            QString asset_id_string/*=QString("")*/)
{
    QMutexLocker locker(&m_Mutex);

    int nContactID = 0;

    QString str_select = QString("SELECT nym.contact_id "
                                 "FROM nym_account "
                                 "INNER JOIN nym "
                                 "ON nym_account.nym_id=nym.nym_id "
                                 "WHERE nym_account.account_id=%1;").arg(acct_id_string);

    int nRows = DBHandler::getInstance()->querySize(str_select);

    for(int ii=0; ii < nRows; ii++)
    {
        //Extract data
        nContactID = DBHandler::getInstance()->queryInt(str_select, 0, ii);
        break;

        // IN THIS CASE, the account record already existed for the given account ID.
        // We were able to join it to the Nym table and get the contact ID as well.
        // BUT -- what if the account record is incomplete? For example, what if
        // the server ID, or asset type ID fields were blank for that account?
        //
        // If they were in the record already, but blank in this call, we'd
        // want to discard the blank values and keep the existing record. But
        // if they were blank in the record, yet available here in this call,
        // then we'd want to UPDATE the record with the latest available values.
        // We certainly wouldn't want to just return and leave the record blank,
        // when we DID have good values that good have been set.
        //
        // UPDATE: They shouldn't ever be blank, because when the account ID is
        // available, then the server and asset type also are (so they would have
        // been added.) The only ID that could be missing would be a NymID,
        // and that wouldn't put us in this block due to the JOIN on NymID.
        // Therefore BELOW this block, we have to check for cases where the account
        // exists even when the Nym doesn't, and if the Nym ID was passed in,
        // add the new Nym, and update the account record with that Nym ID.
        // Of course in that case, we are adding a Nym who has no corresponding
        // contact record, but if he ever IS added as a contact, we will be able
        // to display his name next to his account-related receipts, even those
        // which have no Nym ID associated, since those that DID will enable us
        // to look up later the ones that DON'T.
        //
    }
    // ---------------------------------
    // The above only works if we already have a record of the account AND Nym.
    // But let's say this account is new -- no record of it, even though we
    // may already know the NYM who owns this account.
    //
    // Therefore we need to check to see if the Nym is on a contact, who owns
    // this account, and if so, we need to add this account under that Nym,
    // and return the contact ID. Although it may be in reverse -- we may
    // know the account and Nym IDs now, even though we previously had a record
    // for one or the other, and up until now had no way of connecting the two.
    //
    // However, we need to add the account one way or the other, so we'll do
    // that first, and then if the following piece applies, it can always
    // assume to do an update instead of an insert.
    // ---------------------------------
    QString final_server_id = server_id_string;
    QString final_nym_id    = nym_id_string;
    // ---------------------------------
    QString str_select_acct = QString("SELECT (`server_id`, `asset_id`, `nym_id`) "
                                      "FROM `nym_account` "
                                      "WHERE `account_id`=%1;").arg(acct_id_string);

    int nRowsAcct = DBHandler::getInstance()->querySize(str_select_acct);

    if (nRowsAcct > 0) // If the account record already existed.
    {
        // Update it IF we have values worth sticking in there.
        //
        if (!server_id_string.isEmpty() || !asset_id_string.isEmpty() || !nym_id_string.isEmpty())
        {
            QString existing_server_id = DBHandler::getInstance()->queryString(str_select_acct, 0, 0);
            QString existing_asset_id  = DBHandler::getInstance()->queryString(str_select_acct, 1, 0);
            QString existing_nym_id    = DBHandler::getInstance()->queryString(str_select_acct, 2, 0);

            // Here we're just making sure we don't run an update unless we've
            // actually added some new data.
            //
            if ((existing_server_id.isEmpty() && !server_id_string.isEmpty()) ||
                (existing_asset_id.isEmpty()  && !asset_id_string.isEmpty())  ||
                (existing_asset_id.isEmpty()  && !asset_id_string.isEmpty()) )
            {
                        final_server_id    = !existing_server_id.isEmpty() ? existing_server_id : server_id_string;
                QString final_asset_id     = !existing_asset_id.isEmpty()  ? existing_asset_id  : asset_id_string;
                        final_nym_id       = !existing_nym_id.isEmpty()    ? existing_nym_id    : nym_id_string;
                // -----------------------------------------------------------------
                QString str_update_acct = QString("UPDATE nym_account SET `server_id`='%1',`asset_id`='%2',`nym_id`='%3' WHERE account_id=%4").
                        arg(final_server_id).arg(final_asset_id).arg(final_nym_id).arg(acct_id_string);

                DBHandler::getInstance()->runQuery(str_update_acct);
            }
        }
    }
    else // the account record didn't already exist.
    {
        // Add it then.
        //
        QString str_insert_acct = QString("INSERT INTO `nym_account` "
                                          "(`account_id`, `server_id`, `nym_id`, `asset_id`) "
                                          "VALUES('%1', '%2', '%3', '%4')").arg(acct_id_string).arg(server_id_string).arg(nym_id_string).arg(asset_id_string);
        DBHandler::getInstance()->runQuery(str_insert_acct);
    }
    // By this point, the record of this account definitely exists, though we may not have previously
    // had a record of it. (Thus below, we can assume to update, rather than insert, such a record.)
    // ---------------------------------
    // Here's where we check to see if the Nym exists for this account,
    // for any other existing contact, even though the account itself
    // may not have previously existed.
    //
    if (!final_nym_id.isEmpty())
    {
        QString str_select_nym = QString("SELECT `contact_id` FROM `nym` WHERE nym_id=%1 LIMIT 0,1").arg(final_nym_id);

        int nRowsNym = DBHandler::getInstance()->querySize(str_select_nym);

        if (nRowsNym > 0) // the nymId was found!
        {
            for(int ii=0; ii < nRowsNym; ii++)
            {
                // Found it! (If we're in this loop.) This means a Contact was found in the contact db
                // who already contained a Nym with an ID matching the NymID on this account.
                //
                nContactID = DBHandler::getInstance()->queryInt(str_select_nym, 0, ii);
                // ------------------------------------------------------------
                // So we definitely found the right contact and should return it.
                //
                break; // In practice there should only be one row.

            } // For (nym existed, even though account didn't.)
        }
        // -------------------------
        // else... nym record wasn't found, but we DO have the Nym ID as well as the account ID,
        // and we HAVE already added a record for the account. So perhaps we should add a
        // record for the Nym as well...
        //
        else
        {
            QString str_insert_nym = QString("INSERT INTO `nym` "
                                             "(`nym_id`) "
                                             "VALUES('%1')").arg(final_nym_id);
            DBHandler::getInstance()->runQuery(str_insert_nym);
        }
        // ---------------------------------------------------------------------
        // Finally, do the same actions found in NotifyOfNymServerPair, only if final_server_id
        // !isEmpty(), to make sure we record the server as well, when appropriate.
        //
        if (!final_server_id.isEmpty())
        {
            QString str_select_server = QString("SELECT `server_id` FROM `nym_server` WHERE nym_id=%1 AND server_id=%2 LIMIT 0,1").arg(final_nym_id).arg(final_server_id);
            int nRowsServer = DBHandler::getInstance()->querySize(str_select_server);

            if (0 == nRowsServer) // It wasn't already there. (Add it.)
            {
                QString str_insert_server = QString("INSERT INTO `nym_server` "
                                                    "(`nym_id`, `server_id`) "
                                                    "VALUES('%1', '%2')").arg(final_nym_id).arg(final_server_id);
                DBHandler::getInstance()->runQuery(str_insert_server);
            }
        }
    } // NymID is available (was passed in.)
    // ---------------------------------
    return nContactID;
}



int MTContactHandler::FindContactIDByNymID(QString nym_id_string)
{
    QMutexLocker locker(&m_Mutex);

    QString str_select = QString("SELECT `contact_id` FROM `nym` WHERE nym_id=%1").arg(nym_id_string);

    int nRows = DBHandler::getInstance()->querySize(str_select);

    for(int ii=0; ii < nRows; ii++)
    {
        //Extract data
        int contact_id = DBHandler::getInstance()->queryInt(str_select, 0, ii);

        return contact_id; // In practice there should only be one row.
    }

    return 0; // Didn't find anyone.
}


MTContactHandler::~MTContactHandler()
{
}




